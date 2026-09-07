#pragma once
#include <vector>
#include <memory>
#include <span>
#include <onnxruntime_cxx_api.h>

inline constexpr size_t INPUT_SIZE = 1 * 6 * 256 * 256;
inline constexpr size_t OUTPUT_SIZE = 1 * 3 * 256 * 256;

class MobileUNet 
{
    public:
        MobileUNet() = default;
        ~MobileUNet() = default;
        bool InitializeModelFromBuffer(const void* modelData, size_t modelSize); 
        bool UNetInference(
            std::span<const float, INPUT_SIZE> input,
            std::span<float, OUTPUT_SIZE> imgDelta
        );
        bool IsInitialized() const { return session_ != nullptr; } 

    private:
        Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "MobileUnet"};
        Ort::SessionOptions sessionOptions_;
        std::unique_ptr<Ort::Session> session_;
        Ort::MemoryInfo memoryInfo_{Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)};
};