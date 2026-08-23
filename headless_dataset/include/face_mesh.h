// headers
#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <spdlog/spdlog.h>
#include "face_mesh_triangles.h"
#include "face_landmarker.h"

// forward declarations to mp structures (as opaque pointers)
struct MpFaceLandmarkerInternal;
typedef struct MpFaceLandmarkerInternal* MpFaceLandmarkerPtr;

struct MpImageInternal;
typedef struct MpImageInternal* MpImagePtr;

class FaceMesh
{
public:
    FaceMesh();
    ~FaceMesh();

    bool InitializeFaceLandmarker(const std::string& modelPath);

    bool DetectLandmarks(
        const uint8_t* rgbaData,
        int width,
        int height,
        std::vector<MpNormalizedLandmark>& outLandmarks
    );

    const std::string& GetError() const { return error_; }

private:
    MpFaceLandmarkerPtr landmarker_ = nullptr;
    std::string error_;
    std::vector<MpNormalizedLandmark> landmarks_;
};