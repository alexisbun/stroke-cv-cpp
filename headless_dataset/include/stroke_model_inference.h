#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
// #include <onnxruntime_cxx_api.h>
#include "face_mesh.h"

class StrokeModelInference
{
    public:
        StrokeModelInference();
        ~StrokeModelInference() = default;

        bool InitializeModelFromBuffer(const void* modelData, size_t modelSize);
        bool PredictStrokeLandmarks(const std::vector<MpNormalizedLandmark>& rawInputLandmarks, std::vector<MpNormalizedLandmark>& outStrokeLandmarks);
    private:
};