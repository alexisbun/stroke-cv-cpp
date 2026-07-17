#pragma once

#include <android/hardware_buffer.h>
#include <vector>
#include <mutex>
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
    bool landmarksUpdated_ = false;

    // callback for result_callback_fn
    static void onFaceLandmarksReady(
        MpStatus status,
        const MpFaceLandmarkerResult* result,
        MpImagePtr image,
        int64_t timestamp_ms
    );

    void handleResult(MpStatus status, const MpFaceLandmarkerResult* result);


};