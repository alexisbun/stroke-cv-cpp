#pragma once

#include <android/hardware_buffer.h>
#include <vector>
#include <mutex>
#include "face_landmarker.h"
#include <cstdint>
#include <cstddef>

namespace {
    // Landmark index array for AR effect 
    const uint16_t FACE_MESH_TRIANGLES[] = {
        // Left eye region 
        33, 7, 163,    7, 144, 163,   144, 145, 153,
        
        // Lip region 
        61, 185, 40,   61, 40, 39,    61, 39, 37,    0, 37, 267,
        
        // Cheek and nose region 
        1, 2, 98,      2, 327, 98,    327, 460, 98,
    };
    constexpr size_t NUM_FACE_INDICES = sizeof(FACE_MESH_TRIANGLES) / sizeof(uint16_t); // 2688
}


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

    bool InitializeFaceLandmarkerFromBuffer(const char* buffer, size_t size);
    void ProcessFrame(AHardwareBuffer* src_hardware_buffer, int64_t timestamp_ms);
    bool GetLatestLandmarks(std::vector<MpNormalizedLandmark>& out_landmarks);
    const std::string& GetLastError() const { return lastError_; };

private:
    MpFaceLandmarkerPtr landmarker_ = nullptr;
    std::string lastError_;
    char* modelBuffer_ = nullptr;

    std::vector<uint8_t> scaledBuffer_;
    std::vector<uint8_t> fullResolutionArgbBuffer_;
    int scaledWidth_ = 256;
    int scaledHeight_ = 256;

    std::mutex landmarksMutex_;
    std::vector<MpNormalizedLandmark> latestLandmarks_;

    static void onFaceLandmarksReady(
        MpStatus status,
        const MpFaceLandmarkerResult* result,
        MpImagePtr image,
        int64_t timestamp_ms
    );

    void handleResult(MpStatus status, const MpFaceLandmarkerResult* result);


};