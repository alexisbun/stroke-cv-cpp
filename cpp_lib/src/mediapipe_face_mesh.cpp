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
    options.min_face_detection_confidence = 0.15f;
    options.min_face_presence_confidence = 0.15f;
    options.min_tracking_confidence = 0.15f;
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

// Utilities for computing the face ROI for ONNX inference
struct FaceROI 
{
    int x = 0;
    int y = 0;
    int size = 0;
};

inline FaceROI ComputeFaceROI(const std::vector<MpNormalizedLandmark>& landmarks, int imgW, int imgH) 
{
    if (landmarks.empty() || imgW <= 0 || imgH <= 0) return FaceROI{};

    float minX = 1.0f;
    float maxX = 0.0f;
    float minY = 1.0f;
    float maxY = 0.0f;

    for (const auto& lm : landmarks) {
        minX = std::min(minX, lm.x);
        maxX = std::max(maxX, lm.x);
        minY = std::min(minY, lm.y);
        maxY = std::max(maxY, lm.y);
    }

    float faceW = (maxX - minX) * static_cast<float>(imgW);
    float faceH = (maxY - minY) * static_cast<float>(imgH);
    int size = static_cast<int>(std::round(std::max(faceW, faceH) * 1.5f));
    size = std::min({size, imgW, imgH});

    int centerX = static_cast<int>(std::round((minX + maxX) * 0.5f * imgW));
    int centerY = static_cast<int>(std::round((minY + maxY) * 0.5f * imgH));

    int x = std::clamp(centerX - size / 2, 0, imgW - size);
    int y = std::clamp(centerY - size / 2, 0, imgH - size);

    return FaceROI{x, y, size};
}

inline void PlanarToRBB(const float* planarDelta, uint8_t* outRgb256) {
    constexpr int PIXELS = 256 * 256;
    const float* r = planarDelta;
    const float* g = planarDelta + PIXELS;
    const float* b = planarDelta + PIXELS * 2;
    for (int i = 0; i < PIXELS; ++i) {
        outRgb256[i * 3 + 0] = static_cast<uint8_t>(std::clamp((r[i] + 1.0f) * 127.5f, 0.0f, 255.0f));
        outRgb256[i * 3 + 1] = static_cast<uint8_t>(std::clamp((g[i] + 1.0f) * 127.5f, 0.0f, 255.0f));
        outRgb256[i * 3 + 2] = static_cast<uint8_t>(std::clamp((b[i] + 1.0f) * 127.5f, 0.0f, 255.0f));
    }
}