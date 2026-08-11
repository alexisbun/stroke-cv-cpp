#pragma once
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <onnxruntime_cxx_api.h>
#include "mediapipe_face_mesh.h"

class StrokeModelInference
{
public:
    StrokeModelInference();
    ~StrokeModelInference() = default;

    bool InitializeModelFromBuffer(const void* modelData, size_t modelSize);

    bool PredictStrokeLandmarks(
        const std::vector<MpNormalizedLandmark>& rawInputLandmarks,
        std::vector<MpNormalizedLandmark>& outStrokeLandmarks,
        float intensity = 1.0f);

private:
    Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "StrokeModelInference"};
    Ort::SessionOptions sessionOptions_;
    std::unique_ptr<Ort::Session> session_;
    Ort::MemoryInfo memoryInfo_{Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)};

    std::vector<float> inputTensorValues_;
    std::vector<float> outputDeltaValues_;
    std::vector<float> boundaryWeights_;
    std::vector<float> prevDisplacement_;  

    bool isFirstFrame_ = true;
    float smoothingAlpha_ = 0.12f;

    void InitializeBoundaryWeights();
};