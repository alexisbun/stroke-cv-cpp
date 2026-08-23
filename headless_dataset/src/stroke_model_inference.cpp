#include "stroke_model_inference.h"


StrokeModelInference::StrokeModelInference()
{
    inputTensorValues_.resize(1 * 478 * 3, 0.0f);
}

bool StrokeModelInference::InitializeModel(const std::string& modelPath)
{
    try {
        sessionOptions_.SetIntraOpNumThreads(2);
        sessionOptions_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        session_ = std::make_unique<Ort::Session>(env_, modelPath.c_str(), sessionOptions_);
        spdlog::info("StrokeModelInference: Initialized ONNX model from {}", modelPath);
        return true;
    } catch (const Ort::Exception& e) {
        error_ = e.what();
        spdlog::error("StrokeModelInference Init Error: {}", error_);
        return false;
    }
}

bool StrokeModelInference::PredictStrokeLandmarks(
    const std::vector<MpNormalizedLandmark>& rawInputLandmarks,
    std::vector<MpNormalizedLandmark>& outStrokeLandmarks
)
{
    if (!session_) {
        error_ = "ONNX Session is not initialized.";
        spdlog::error("StrokeModelInference: {}", error_);
        return false;
    }
    if (rawInputLandmarks.size() < 478) {
        error_ = "Input landmark count is less than 478.";
        spdlog::error("StrokeModelInference: {}", error_);
        return false;
    }

    outStrokeLandmarks.resize(rawInputLandmarks.size());

    // scale invariance normalization
    const auto& nose = rawInputLandmarks[4];
    const auto& leftPupil = rawInputLandmarks[468];
    const auto& rightPupil = rawInputLandmarks[473];

    float centerX = nose.x;
    float centerY = nose.y;
    float centerZ = nose.z;

    float dx = leftPupil.x - rightPupil.x;
    float dy = leftPupil.y - rightPupil.y;
    float dz = leftPupil.z - rightPupil.z;
    float scale = std::sqrt(dx * dx + dy * dy + dz * dz);

    // (raw - center) / scale
    for (size_t i = 0; i < 478; ++i) {
        inputTensorValues_[i * 3 + 0] = (rawInputLandmarks[i].x - centerX) / scale;
        inputTensorValues_[i * 3 + 1] = (rawInputLandmarks[i].y - centerY) / scale;
        inputTensorValues_[i * 3 + 2] = (rawInputLandmarks[i].z - centerZ) / scale;
    }

    const char* inputNames[]  = {"input_landmarks"};
    const char* outputNames[] = {"transformed_landmarks", "displacement_deltas"};
    std::vector<int64_t> inputDims = {1, 478, 3};
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo_,
        inputTensorValues_.data(),
        inputTensorValues_.size(),
        inputDims.data(),
        inputDims.size()
    );

    auto outputTensors = session_->Run(
        Ort::RunOptions{nullptr},
        inputNames,
        &inputTensor,
        1,
        outputNames,
        2
    );

    float* rawDeltaPtr = outputTensors[1].GetTensorMutableData<float>();

    for (size_t i = 0; i < 478; ++i) {
        float deltaX = rawDeltaPtr[i * 3 + 0] * scale;
        float deltaY = rawDeltaPtr[i * 3 + 1] * scale;
        float deltaZ = rawDeltaPtr[i * 3 + 2] * scale;
        outStrokeLandmarks[i].x = rawInputLandmarks[i].x + deltaX;
        outStrokeLandmarks[i].y = rawInputLandmarks[i].y + deltaY;
        outStrokeLandmarks[i].z = rawInputLandmarks[i].z + deltaZ;
    }

    return true;
}