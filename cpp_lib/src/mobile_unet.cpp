#include "mobile_unet.h"
#include <spdlog/spdlog.h>

namespace {
    constexpr std::array<int64_t, 4> INPUT_DIMENTIONS = {1, 6, 256, 256};
    constexpr std::array<int64_t, 4> OUTPUT_DIMENTIONS = {1, 3, 256, 256};
}

bool MobileUNet::InitializeModelFromBuffer(const void* modelData, size_t modelSize)
{
    try {
        sessionOptions_.SetInterOpNumThreads(2);
        sessionOptions_.SetIntraOpNumThreads(2);
        sessionOptions_.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        session_ = std::make_unique<Ort::Session>(
            env_, modelData, modelSize, sessionOptions_
        );

        spdlog::info("ONNX Runtime: MobileUNet initialized successfully!");
        return true;
    } catch (const Ort::Exception& e) {
        spdlog::error("ONNX Runtime: MobileUNet threw exception during initialization {} ", e.what());
        return false;
    }
}

bool MobileUNet::UNetInference(
    std::span<const float, INPUT_SIZE> input,
    std::span<float, OUTPUT_SIZE> imgDelta) 
{
    if (!session_) return false;

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo_,
        const_cast<float*>(input.data()),
        input.size(),
        INPUT_DIMENTIONS.data(),
        INPUT_DIMENTIONS.size()
    );

    Ort::Value outputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo_,
        imgDelta.data(),
        imgDelta.size(),
        OUTPUT_DIMENTIONS.data(),
        OUTPUT_DIMENTIONS.size()
    );

    Ort::IoBinding ioBinding(*session_);
    ioBinding.BindInput("input", inputTensor);
    ioBinding.BindOutput("delta", outputTensor);

    session_->Run(Ort::RunOptions{nullptr}, ioBinding);

}