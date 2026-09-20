#include "stroke_model_inference.h"
#include "face_mesh_triangles.h"
#include <spdlog/spdlog.h>

StrokeModelInference::StrokeModelInference()
{   
    // pre-allocation of heap memory for input tensors, displacement vectors, prev displacement.
    inputTensorValues_.resize(1 * 478 * 3, 0.0f);
    outputDeltaValues_.resize(1 * 478 * 3, 0.0f);
    prevDisplacement_.resize(478 * 3, 0.0f);
    InitializeBoundaryWeights(); 
    InitializeMeshTopology();
}

void StrokeModelInference::InitializeMeshTopology()
{
    meshNeighbors_.assign(478, {});
    constexpr size_t numTriangles = NUM_FACE_INDICES / 3;
    for (size_t t = 0; t < numTriangles; ++t) {
        uint16_t i0 = FACE_MESH_TRIANGLES[t * 3 + 0];
        uint16_t i1 = FACE_MESH_TRIANGLES[t * 3 + 1];
        uint16_t i2 = FACE_MESH_TRIANGLES[t * 3 + 2];
        if (i0 >= 478 || i1 >= 478 || i2 >= 478) continue;

        auto addNeighbor = [this](int u, int v) {
            if (std::find(meshNeighbors_[u].begin(), meshNeighbors_[u].end(), v) == meshNeighbors_[u].end()) {
                meshNeighbors_[u].push_back(v);
            }
        };

        addNeighbor(i0, i1); addNeighbor(i0, i2);
        addNeighbor(i1, i0); addNeighbor(i1, i2);
        addNeighbor(i2, i0); addNeighbor(i2, i1);
    }
}

void StrokeModelInference::InitializeBoundaryWeights()
{
    boundaryWeights_.assign(478, 1.0f);
    
    const std::vector<int> faceOvalIndicies = {
        10, 338, 297, 332, 284, 251, 389, 356, 454, 323, 361, 288,
        397, 365, 379, 378, 400, 377, 152, 148, 176, 149, 150, 136,
        172, 58,  132, 93,  234, 127, 162, 21,  54,  103, 67,  109
    };
    for (int idx : faceOvalIndicies) {
        if (idx >= 0 && idx < 478) {
            boundaryWeights_[idx] = 0.0f;
        }
    }

    const std::vector<int> upperFaceIndicies = {
        // Forehead 
        10, 151, 9, 8, 168, 6, 197, 195, 5,
        // Eyebrows
        70, 63, 105, 66, 107, 55, 65, 52, 53, 46,
        300, 293, 334, 296, 336, 285, 295, 282, 283, 276,
        // Eyes
        33, 7, 163, 144, 145, 153, 154, 155, 133, 246, 161, 160, 159, 158, 157, 173,
        263, 249, 390, 373, 374, 380, 381, 382, 362, 466, 388, 387, 386, 385, 384, 398,
        468, 469, 470, 471, 472, 473, 474, 475, 476, 477
    };
    for (int idx : upperFaceIndicies) {
        if (idx >= 0 && idx < 478) {
            boundaryWeights_[idx] = 0.0f;
        }
    }

    const std::vector<int> chinIndicies = {
        18, 200, 199, 175,
        32, 208, 211, 210, 201, 204, 140, 171, 170, 169, 135, 138, 215, 177, 137, 227,
        262, 428, 431, 421, 424, 369, 396, 394, 395, 366, 401, 435, 367, 364
    };
    for (int idx : chinIndicies) {
        if (idx >= 0 && idx < 478) {
            boundaryWeights_[idx] = 0.05f;
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
    std::vector<MpNormalizedLandmark>& strokeLandmarks
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

    float centerUprightX = 1.0f - nose.y;
    float centerUprightY = 1.0f - nose.x;

    float dx = leftPupil.x - rightPupil.x;
    float dy = leftPupil.y - rightPupil.y;
    float dz = leftPupil.z - rightPupil.z;
    float scale = std::sqrt(dx * dx + dy * dy + dz * dz);

    // use formula (raw - center) / scale to normalize input landmark coordinates
    for (size_t i = 0; i < 478; ++i) {
        float uprightX = 1.0f - landmarks[i].y;
        float uprightY = 1.0f - landmarks[i].x;
        inputTensorValues_[i * 3 + 0] = (uprightX - centerUprightX) / scale;
        inputTensorValues_[i * 3 + 1] = (uprightY - centerUprightY) / scale;
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

    std::vector<float> relaxedX(478);
    std::vector<float> relaxedY(478);
    std::vector<float> relaxedZ(478);

    for (size_t i = 0; i < 478; ++i) {
        float w = (i < boundaryWeights_.size()) ? boundaryWeights_[i] : 1.0f;
        float deltaX_upright = rawDeltaPtr[i * 3 + 0] * scale;
        float deltaY_upright = rawDeltaPtr[i * 3 + 1] * scale;
        float deltaZ = rawDeltaPtr[i * 3 + 2] * scale;

        relaxedX[i] = -deltaY_upright * w;
        relaxedY[i] = -deltaX_upright * w;
        relaxedZ[i] = deltaZ * w;
    }

    constexpr int NUM_RELAX_ITERATIONS = 5;
    constexpr float RELAX_ALPHA = 0.05f;

    for (int iter = 0; iter < NUM_RELAX_ITERATIONS; ++iter) {
        std::vector<float> nextX = relaxedX;
        std::vector<float> nextY = relaxedY;
        std::vector<float> nextZ = relaxedZ;

        for (size_t i = 0; i < 478; ++i) {
            if (boundaryWeights_[i] == 0.0f) {
                nextX[i] = 0.0f;
                nextY[i] = 0.0f;
                nextZ[i] = 0.0f;
                continue;
            }

            const auto &nbrs = meshNeighbors_[i];
            if (nbrs.empty()) continue;

            float sumX = 0.0f, sumY = 0.0f, sumZ = 0.0f;
            for (int nbr : nbrs) {
                sumX += relaxedX[nbr];
                sumY += relaxedY[nbr];
                sumZ += relaxedZ[nbr];
            }
            float avgX = sumX / static_cast<float>(nbrs.size());
            float avgY = sumY / static_cast<float>(nbrs.size());
            float avgZ = sumZ / static_cast<float>(nbrs.size());

            nextX[i] = (1.0f - RELAX_ALPHA) * relaxedX[i] + RELAX_ALPHA * avgX;
            nextY[i] = (1.0f - RELAX_ALPHA) * relaxedY[i] + RELAX_ALPHA * avgY;
            nextZ[i] = (1.0f - RELAX_ALPHA) * relaxedZ[i] + RELAX_ALPHA * avgZ;
        }

        relaxedX = std::move(nextX);
        relaxedY = std::move(nextY);
        relaxedZ = std::move(nextZ);
    }

    for (size_t i = 0; i < 478; ++i) {
        float deltaX_cam = relaxedX[i];
        float deltaY_cam = relaxedY[i];
        float deltaZ_cam = relaxedZ[i];

        if (isFirstFrame_) {
            prevDisplacement_[i * 3 + 0] = deltaX_cam;
            prevDisplacement_[i * 3 + 1] = deltaY_cam;
            prevDisplacement_[i * 3 + 2] = deltaZ_cam;
        } else {
            deltaX_cam = smoothingAlpha_ * deltaX_cam + (1.0f - smoothingAlpha_) * prevDisplacement_[i * 3 + 0];
            deltaY_cam = smoothingAlpha_ * deltaY_cam + (1.0f - smoothingAlpha_) * prevDisplacement_[i * 3 + 1];
            deltaZ_cam = smoothingAlpha_ * deltaZ_cam + (1.0f - smoothingAlpha_) * prevDisplacement_[i * 3 + 2];
            prevDisplacement_[i * 3 + 0] = deltaX_cam;
            prevDisplacement_[i * 3 + 1] = deltaY_cam;
            prevDisplacement_[i * 3 + 2] = deltaZ_cam;
        }
        strokeLandmarks[i].x = landmarks[i].x + deltaX_cam;
        strokeLandmarks[i].y = landmarks[i].y + deltaY_cam;
        strokeLandmarks[i].z = landmarks[i].z + deltaZ_cam;
    }
    isFirstFrame_ = false;
    return true;
}