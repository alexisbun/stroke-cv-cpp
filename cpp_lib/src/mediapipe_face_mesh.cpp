#include "mediapipe_face_mesh.h"
#include <libyuv.h>
#include <spdlog/spdlog.h>
#include <cstring>

static FaceMesh* gFaceMeshInstance = nullptr;

FaceMesh::FaceMesh()
{
    gFaceMeshInstance = this;
    scaledBuffer_.resize(scaledWidth_ * scaledHeight_ * 4); // pre-allocate memory for scaledBuffer_
}

FaceMesh::~FaceMesh()
{
    if (gFaceMeshInstance == this) {
        gFaceMeshInstance = nullptr;
    }
    if (landmarker_)
    {
        char* error_msg = nullptr;
        MpFaceLandmarkerClose(landmarker_, &error_msg);
        if (error_msg)
        {
            MpErrorFree(error_msg);
        }
    }
    if (modelBuffer_) {
        delete[] modelBuffer_;
        modelBuffer_ = nullptr;
    }
}

bool FaceMesh::InitializeFaceLandmarkerFromBuffer(const char* buffer, size_t size) {

    if (modelBuffer_) {
        delete[] modelBuffer_;
    }
    modelBuffer_ = const_cast<char*>(buffer);

    struct MpFaceLandmarkerOptions options;
    std::memset(&options, 0, sizeof(options));

    options.base_options.model_asset_buffer = buffer;
    options.base_options.model_asset_buffer_count = size;
    options.base_options.delegate = MP_DELEGATE_GPU;
    options.running_mode = MP_RUNNING_MODE_LIVE_STREAM;
    options.num_faces = 1;
    options.min_face_detection_confidence = 0.5f;
    options.min_face_presence_confidence = 0.5f;
    options.min_tracking_confidence = 0.5f;
    options.output_face_blendshapes = false;
    options.output_facial_transformation_matrixes = true;
    options.result_callback = FaceMesh::onFaceLandmarksReady;

    char* error_msg = nullptr;
    MpStatus status = MpFaceLandmarkerCreate(&options, &landmarker_, &error_msg);

    if (status != kMpOk) {
        lastError_ = error_msg ? error_msg : "Unknown error during initialization";
        spdlog::error("MediaPipe Init Error: {}", lastError_);
        if (error_msg) free(error_msg);
        return false;
    }
    spdlog::info("MediaPipe Face Landmarker initialized.");
    return true;
}


void FaceMesh::ProcessFrame(AHardwareBuffer* hardware_buffer, int64_t timestamp_ms) {
    if (!landmarker_ || !hardware_buffer) return;

    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(hardware_buffer, &desc);

    spdlog::debug("FaceMesh::ProcessFrame: buffer details: width={}, height={}, stride={}, format={}, usage={}",
                  desc.width, desc.height, desc.stride, desc.format, desc.usage);

    if (desc.format == AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420) {
        AHardwareBuffer_Planes planes;
        int lockStatus = AHardwareBuffer_lockPlanes(
            hardware_buffer,
            AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,
            -1,
            nullptr,
            &planes
        );

        if (lockStatus != 0 || planes.planeCount < 3) {
            spdlog::error("FaceMesh::ProcessFrame: Failed to lock YUV planes: {}", lockStatus);
            return;
        }

        size_t fullResSize = desc.width * desc.height * 4;
        if (fullResolutionArgbBuffer_.size() < fullResSize) {
            fullResolutionArgbBuffer_.resize(fullResSize);
        }

        libyuv::Android420ToARGB(
            reinterpret_cast<const uint8_t*>(planes.planes[0].data), planes.planes[0].rowStride,
            reinterpret_cast<const uint8_t*>(planes.planes[1].data), planes.planes[1].rowStride,
            reinterpret_cast<const uint8_t*>(planes.planes[2].data), planes.planes[2].rowStride,
            planes.planes[1].pixelStride,
            fullResolutionArgbBuffer_.data(), desc.width * 4,
            desc.width, desc.height
        );

        AHardwareBuffer_unlock(hardware_buffer, nullptr);

        int dstStrideBytes = scaledWidth_ * 4;
        libyuv::ARGBScale(
            fullResolutionArgbBuffer_.data(), desc.width * 4, desc.width, desc.height,
            scaledBuffer_.data(), dstStrideBytes, scaledWidth_, scaledHeight_,
            libyuv::kFilterBox
        );
    } else if (desc.format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM) {
        void* virtualAddress = nullptr;
        int lockStatus = AHardwareBuffer_lock(
            hardware_buffer,
            AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN,
            -1,
            nullptr,
            &virtualAddress
        );

        if (lockStatus != 0 || !virtualAddress) {
            spdlog::error("Failed to lock AHardwareBuffer: {}", lockStatus);
            return;
        }

        const uint8_t* srcRgba = reinterpret_cast<const uint8_t*>(virtualAddress);
        int srcStrideBytes = desc.stride * 4;
        int dstStrideBytes = scaledWidth_ * 4;

        libyuv::ARGBScale(
            srcRgba, srcStrideBytes, desc.width, desc.height,
            scaledBuffer_.data(), dstStrideBytes, scaledWidth_, scaledHeight_,
            libyuv::kFilterBox
        );

        AHardwareBuffer_unlock(hardware_buffer, nullptr);
    } else {
        spdlog::error("FaceMesh::ProcessFrame: Unsupported buffer format {}", desc.format);
        return;
    }

    MpImagePtr image = nullptr;
    char* error_msg = nullptr;
    MpStatus status = MpImageCreateFromUint8Data(
        kMpImageFormatSrgba,
        scaledWidth_,
        scaledHeight_,
        scaledBuffer_.data(),
        scaledBuffer_.size(),
        &image,
        &error_msg
    );

    if (status != kMpOk) {
        spdlog::error("Failed to create MpImage: {}", error_msg ? error_msg : "");
        if (error_msg) free(error_msg);
        return;
    }

    status = MpFaceLandmarkerDetectAsync(landmarker_, image, nullptr, timestamp_ms, &error_msg);
    if (status != kMpOk) {
        spdlog::error("Failed to run FaceLandmarker: {}", error_msg ? error_msg : "");
        if (error_msg) free(error_msg);
    }

    MpImageFree(image);
}

void FaceMesh::onFaceLandmarksReady(MpStatus status, const MpFaceLandmarkerResult* result, MpImagePtr image, int64_t timestamp_ms) 
{
    if (gFaceMeshInstance) {
        gFaceMeshInstance->handleResult(status, result);
    }
}

void FaceMesh::handleResult(MpStatus status, const MpFaceLandmarkerResult* result) {
    if (status != kMpOk || !result || result->face_landmarks_count == 0) {
        // grab mutex and clear the detections
        std::lock_guard<std::mutex> lock(landmarksMutex_);
        latestLandmarks_.clear();
        return;
    }

    std::lock_guard<std::mutex> lock(landmarksMutex_);
    const auto& face = result->face_landmarks[0];
    latestLandmarks_.assign(face.landmarks, face.landmarks + face.landmarks_count);
}

bool FaceMesh::GetLatestLandmarks(std::vector<MpNormalizedLandmark>& out_landmarks) {
    std::lock_guard<std::mutex> lock(landmarksMutex_);
    if (latestLandmarks_.size() == 0) {
        return false;
    }
    out_landmarks = latestLandmarks_;
    return true;
}