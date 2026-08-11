#include "stroke_model_inference.h"
#include <spdlog/spdlog.h>

StrokeModelInference::StrokeModelInference()
{   
    // pre-allocation of heap memory for input tensors, displacement vectors, prev displacement.
    inputTensorValues_.resize(1 * 478 * 3, 0.0f);
    outputDeltaValues_.resize(1 * 478 * 3, 0.0f);
    prevDisplacement_.resize(478 * 3, 0.0f);
    InitializeBoundaryWeights(); 
}

void StrokeModelInference::InitializeBoundaryWeights()
{
    boundaryWeights_.assign(478, 1.0f);

    const std::vector<int> faceOvalIndicies = {
        10, 338, 297, 332, 284, 251, 389, 356, 454, 323, 361, 288,
        397, 365, 379, 378, 400, 377, 152, 148, 176, 149, 150, 136,
        172, 58, 132, 93, 234, 127, 162, 21, 54, 103, 67, 109,
        151, 337, 298, 333, 299, 334, 296, 336, 285, 8, 55, 107,
        66, 105, 63, 70, 104, 69, 108, 175, 199, 200, 18, 83,
        17, 314, 405, 421, 9,
        33, 7, 163, 144, 145, 153, 154, 155, 133, 246, 161, 160,
        159, 158, 157, 173, 263, 249, 390, 373, 374, 380, 381, 382,
        362, 466, 388, 387, 386, 385, 384, 398, 468, 469, 470, 471,
        472, 473, 474, 475, 476, 477, 46, 53, 52, 65, 70, 63, 105,
        66, 107, 276, 283, 282, 295, 300, 293, 334, 296, 336,
        168, 6, 197, 195, 5, 4, 1, 19, 94, 2
    };

    // ensure that stroke effect isn't applied at face boundry indicies 
    for (int i : faceOvalIndicies) {
        if (i >= 0 && i < 478) {
             boundaryWeights_[i] = 0.0f;
        }
    }
}

bool StrokeModelInference::InitializeModelFromBuffer(const void* modelData, size_t modelSize) 
{
    try {
        sessionOptions_.SetInterOpNumThreads(2);
        sessionOptions_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        session_ = std::make_unique<Ort::Session>(
            env_, modelData, modelSize, sessionOptions_
        );

        spdlog::info("ONNX Runtime: StrokeModelInference initialized successfully.");
        return true;
    } catch(const Ort::Exception& e) {
        spdlog::error("ONNX Runtime Init Exception: {}", e.what());
        return false;
    }
}

bool StrokeModelInference::PredictStrokeLandmarks(
    const std::vector<MpNormalizedLandmark>& landmarks,
    std::vector<MpNormalizedLandmark>& strokeLandmarks,
    float intensity
)
{
    if (!session_ || landmarks.size() < 478) return false;

    strokeLandmarks.resize(landmarks.size());

    // spacial invariance normalization
    const auto& nose = landmarks[4];
    const auto& leftPupil = landmarks[468];
    const auto& rightPupil = landmarks[473];

    float centerX = nose.x;
    float centerY = nose.y;
    float centerZ = nose.z;

    float dx = leftPupil.x - rightPupil.x;
    float dy = leftPupil.y - rightPupil.y;
    float scale = std::sqrt(dx * dx + dy * dy);
    if (scale < 1e-6f) scale = 1e-6f;

    // use formula (raw - center) / scale to normalize input landmark coordinates
    for (size_t i = 0; i < 478; ++i) {
        inputTensorValues_[i * 3 + 0] = (landmarks[i].x - centerX) / scale;
        inputTensorValues_[i * 3 + 1] = (landmarks[i].y - centerY) / scale;
        inputTensorValues_[i * 3 + 2] = (landmarks[i].z - centerZ) / scale;
    }

    const char* inputNames[] = {"input_landmarks"};
    const char* outputNames[] = {"transformed_landmarks", "displacement_deltas"};
    std::vector<int64_t> inputDims = {1, 478, 3};

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo_, inputTensorValues_.data(), inputTensorValues_.size(),
        inputDims.data(), inputDims.size()
    );

    auto outputTensors = session_->Run(
        Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 2
    );

    float* rawDeltaPtr = outputTensors[1].GetTensorMutableData<float>();
    float clampedIntensity = std::clamp(intensity, 0.0f, 3.0f);

    for (size_t i = 0; i < 478; ++i) {
        // un-normalize landmark coordinates
        float weight = boundaryWeights_[i] * clampedIntensity;
        float deltaX = rawDeltaPtr[i * 3 + 0] * scale * weight;
        float deltaY = rawDeltaPtr[i * 3 + 1] * scale * weight;
        float deltaZ = rawDeltaPtr[i * 3 + 2] * scale * weight;
        if (isFirstFrame_) {
            prevDisplacement_[i * 3 + 0] = deltaX;
            prevDisplacement_[i * 3 + 1] = deltaY;
            prevDisplacement_[i * 3 + 2] = deltaZ;
        } else {
            // apply exponential moving average filter to prevent jitter effect (smoothingAlpha_ = 0.2f)
            deltaX = smoothingAlpha_ * deltaX + (1.0f - smoothingAlpha_) * prevDisplacement_[i * 3 + 0];
            deltaY = smoothingAlpha_ * deltaY + (1.0f - smoothingAlpha_) * prevDisplacement_[i * 3 + 1];
            deltaZ = smoothingAlpha_ * deltaZ + (1.0f - smoothingAlpha_) * prevDisplacement_[i * 3 + 2];
            prevDisplacement_[i * 3 + 0] = deltaX;
            prevDisplacement_[i * 3 + 1] = deltaY;
            prevDisplacement_[i * 3 + 2] = deltaZ;
        }
        strokeLandmarks[i].x = landmarks[i].x + deltaX;
        strokeLandmarks[i].y = landmarks[i].y + deltaY;
        strokeLandmarks[i].z = landmarks[i].z + deltaZ;
    }

    isFirstFrame_ = false;
    return true;
}