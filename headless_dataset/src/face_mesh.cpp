#include "face_mesh.h"

FaceMesh::FaceMesh() = default;

FaceMesh::~FaceMesh() 
{
    if (landmarker_) {
        char* errorMsg = nullptr;
        MpStatus status = MpFaceLandmarkerClose(landmarker_, &errorMsg);
        if (status != kMpOk && errorMsg) {
            spdlog::info("FaceMesh: MpFaceLandmarkerClose warning: {}", errorMsg);
            MpErrorFree(errorMsg);
        }
        landmarker_ = nullptr;
    }
}

bool FaceMesh::InitializeFaceLandmarker(const std::string& modelPath)
{
    if (landmarker_) {
        char* errorMsg = nullptr;
        MpFaceLandmarkerClose(landmarker_, &errorMsg);
        if (errorMsg) MpErrorFree(errorMsg);
        landmarker_ = nullptr;
    }

    struct MpFaceLandmarkerOptions options;
    std::memset(&options, 0, sizeof(options));

    options.base_options.model_asset_path = modelPath.c_str();
    options.base_options.delegate = MP_DELEGATE_CPU;
    options.running_mode = MP_RUNNING_MODE_IMAGE;
    options.num_faces = 1;
    options.min_face_detection_confidence = 0.5f;
    options.min_face_presence_confidence = 0.5f;
    options.min_tracking_confidence = 0.5f;
    options.output_face_blendshapes = false;
    options.output_facial_transformation_matrixes = false;
    options.result_callback = nullptr;

    char* errorMsg = nullptr;
    MpStatus status = MpFaceLandmarkerCreate(&options, &landmarker_, &errorMsg);

    if (status != kMpOk || !landmarker_) {
        error_ = errorMsg ? std::string(errorMsg) : "Unknown error creating Face Landmarker from " + modelPath;
        spdlog::info("FaceMesh: {}", error_);
        if (errorMsg) MpErrorFree(errorMsg);
        return false;
    }
    spdlog::info("FaceMesh: Initialized from {}", modelPath);
    return true;
}

bool FaceMesh::DetectLandmarks(
    const uint8_t* rgbaData,
    int width,
    int height,
    std::vector<MpNormalizedLandmark>& outLandmarks)
{
    outLandmarks.clear();

    if (!landmarker_) {
        error_ = "FaceMesh: Landmarker is not initialized.";
        spdlog::info(error_);
        return false;
    }

    if (!rgbaData || width <= 0 || height <= 0) {
        error_ = "FaceMesh: Invalid image dimensions or null data pointer.";
        spdlog::info(error_);
        return false;
    }

    MpImagePtr image = nullptr;
    char* errorMsg = nullptr;
    size_t dataSizeBytes = static_cast<size_t>(width) * height * 4;

    MpStatus status = MpImageCreateFromUint8Data(
        kMpImageFormatSrgba,
        width,
        height,
        rgbaData,
        dataSizeBytes,
        &image,
        &errorMsg
    );

    if (status != kMpOk || !image) {
        error_ = errorMsg ? std::string(errorMsg) : "Failed to create MpImage container.";
        spdlog::error("FaceMesh: {}", error_);
        if (errorMsg) MpErrorFree(errorMsg);
        return false;
    }

    MpFaceLandmarkerResult result;
    std::memset(&result, 0, sizeof(result));

    status = MpFaceLandmarkerDetectImage(
        landmarker_,
        image,
        nullptr,
        &result,
        &errorMsg
    );
    
    MpImageFree(image);

    if (status != kMpOk) {
        error_ = errorMsg ? std::string(errorMsg) : "Face landmark detection failed.";
        spdlog::info("FaceMesh: {}", error_);
        if (errorMsg) MpErrorFree(errorMsg);
        MpFaceLandmarkerCloseResult(&result);
        return false;
    }

    if (result.face_landmarks_count == 0 || !result.face_landmarks) {
        error_ = "FaceMesh: No face detected in image.";
        MpFaceLandmarkerCloseResult(&result);
        return false;
    }

    const auto& face = result.face_landmarks[0];
    if (face.landmarks_count < 478 || !face.landmarks) {
        error_ = "FaceMesh: Incomplete mesh detected (fewer than 478 landmarks).";
        spdlog::warn(error_);
        MpFaceLandmarkerCloseResult(&result);
        return false;
    }

    outLandmarks.assign(face.landmarks, face.landmarks + face.landmarks_count);
    landmarks_ = outLandmarks;

    MpFaceLandmarkerCloseResult(&result);
    return true;
}