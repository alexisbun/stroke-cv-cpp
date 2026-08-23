#pragma once
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <onnxruntime_cxx_api.h>
#include <spdlog/spdlog.h>
#include "face_mesh.h"

class StrokeModelInference
{
    public:
        StrokeModelInference();
        ~StrokeModelInference() = default;

        bool InitializeModel(const std::string& modelPath);
        bool PredictStrokeLandmarks(const std::vector<MpNormalizedLandmark>& rawInputLandmarks, std::vector<MpNormalizedLandmark>& outStrokeLandmarks);

        const std::string& GetError() const { return error_; }
    private:
        Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "StrokeModelInference"};
        Ort::SessionOptions sessionOptions_;
        std::unique_ptr<Ort::Session> session_;
        Ort::MemoryInfo memoryInfo_{Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)};
        std::vector<float> inputTensorValues_;
        std::string error_;
};