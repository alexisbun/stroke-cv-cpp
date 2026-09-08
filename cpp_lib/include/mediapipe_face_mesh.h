#pragma once

#include "face_landmarker.h"
#include <algorithm>
#include <android/hardware_buffer.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <libyuv.h>
#include <mutex>
#include <vector>

// forward declarations to mp structures (as opaque pointers)
struct MpFaceLandmarkerInternal;
typedef struct MpFaceLandmarkerInternal *MpFaceLandmarkerPtr;

struct MpImageInternal;
typedef struct MpImageInternal *MpImagePtr;

class FaceMesh {
public:
  FaceMesh();
  ~FaceMesh();

  bool InitializeFaceLandmarkerFromBuffer(const char *buffer, size_t size);
  void ProcessFrame(AHardwareBuffer *src_hardware_buffer, int64_t timestamp_ms);
  bool GetLatestLandmarks(std::vector<MpNormalizedLandmark> &out_landmarks);
  const std::string &GetLastError() const { return lastError_; };

private:
  MpFaceLandmarkerPtr landmarker_ = nullptr;
  std::string lastError_;
  char *modelBuffer_ = nullptr;

  std::vector<uint8_t> scaledBuffer_;
  std::vector<uint8_t> fullResolutionArgbBuffer_;
  int scaledWidth_ = 256;
  int scaledHeight_ = 256;

  std::mutex landmarksMutex_;
  std::vector<MpNormalizedLandmark> latestLandmarks_;

  static void onFaceLandmarksReady(MpStatus status,
                                   const MpFaceLandmarkerResult *result,
                                   MpImagePtr image, int64_t timestamp_ms);

  void handleResult(MpStatus status, const MpFaceLandmarkerResult *result);
};

// Utilities for computing the face ROI for ONNX inference
struct FaceROI {
  int x = 0;
  int y = 0;
  int size = 0;
};

inline FaceROI
ComputeFaceROI(const std::vector<MpNormalizedLandmark> &landmarks, int imgW,
               int imgH) {
  if (landmarks.empty() || imgW <= 0 || imgH <= 0)
    return FaceROI{};

  float minX = 1.0f;
  float maxX = 0.0f;
  float minY = 1.0f;
  float maxY = 0.0f;

  for (const auto &lm : landmarks) {
    minX = std::min(minX, lm.x);
    maxX = std::max(maxX, lm.x);
    minY = std::min(minY, lm.y);
    maxY = std::max(maxY, lm.y);
  }

  float faceW = (maxX - minX) * static_cast<float>(imgW);
  float faceH = (maxY - minY) * static_cast<float>(imgH);
  int size = static_cast<int>(std::round(std::max(faceW, faceH) * 1.5f));
  size = std::min({size, imgW, imgH});

  int centerX = static_cast<int>(std::round((minX + maxX) * 0.5f * imgW));
  int centerY = static_cast<int>(std::round((minY + maxY) * 0.5f * imgH));

  int x = std::clamp(centerX - size / 2, 0, imgW - size);
  int y = std::clamp(centerY - size / 2, 0, imgH - size);

  return FaceROI{x, y, size};
}

inline void PlanarToRBB(const float *planarDelta, uint8_t *outRgb256) {
  constexpr int PIXELS = 256 * 256;
  const float *r = planarDelta;
  const float *g = planarDelta + PIXELS;
  const float *b = planarDelta + PIXELS * 2;
  for (int i = 0; i < PIXELS; ++i) {
    outRgb256[i * 3 + 0] =
        static_cast<uint8_t>(std::clamp((r[i] + 1.0f) * 127.5f, 0.0f, 255.0f));
    outRgb256[i * 3 + 1] =
        static_cast<uint8_t>(std::clamp((g[i] + 1.0f) * 127.5f, 0.0f, 255.0f));
    outRgb256[i * 3 + 2] =
        static_cast<uint8_t>(std::clamp((b[i] + 1.0f) * 127.5f, 0.0f, 255.0f));
  }
}

inline bool CropAndResizeHardwareBuffer(AHardwareBuffer *buffer,
                                        const FaceROI &roi,
                                        uint8_t *dstRgb256) {
  if (!buffer || roi.size <= 0 || !dstRgb256)
    return false;

  AHardwareBuffer_Desc desc;
  AHardwareBuffer_describe(buffer, &desc);

  AHardwareBuffer_Planes planes;
  int status = AHardwareBuffer_lockPlanes(
      buffer, AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN, -1, nullptr, &planes);
  if (status != 0 || planes.planeCount < 3) {
    if (status == 0)
      AHardwareBuffer_unlock(buffer, nullptr);
    return false;
  }

  int rx = std::clamp(roi.x & ~1, 0, static_cast<int>(desc.width) - 2);
  int ry = std::clamp(roi.y & ~1, 0, static_cast<int>(desc.height) - 2);
  int rSize =
      std::clamp(roi.size & ~1, 2,
                 static_cast<int>(std::min(desc.width - rx, desc.height - ry)));

  const uint8_t *yPlane =
      reinterpret_cast<const uint8_t *>(planes.planes[0].data);
  const uint8_t *uPlane =
      reinterpret_cast<const uint8_t *>(planes.planes[1].data);
  const uint8_t *vPlane =
      reinterpret_cast<const uint8_t *>(planes.planes[2].data);

  int yStride = planes.planes[0].rowStride;
  int uStride = planes.planes[1].rowStride;
  int vStride = planes.planes[2].rowStride;
  int uvPixelStride = planes.planes[1].pixelStride;

  const uint8_t *cropY = yPlane + ry * yStride + rx;
  const uint8_t *cropU = uPlane + (ry / 2) * uStride + (rx / 2) * uvPixelStride;
  const uint8_t *cropV = vPlane + (ry / 2) * vStride + (rx / 2) * uvPixelStride;

  std::vector<uint8_t> cropRgba(rSize * rSize * 4);
  libyuv::Android420ToABGR(cropY, yStride, cropU, uStride, cropV, vStride,
                           uvPixelStride, cropRgba.data(), rSize * 4, rSize,
                           rSize);

  AHardwareBuffer_unlock(buffer, nullptr);

  std::vector<uint8_t> scaledRgba(256 * 256 * 4);
  libyuv::ARGBScale(cropRgba.data(), rSize * 4, rSize, rSize, scaledRgba.data(),
                    256 * 4, 256, 256, libyuv::kFilterBilinear);

  for (int i = 0; i < 256 * 256; ++i) {
    dstRgb256[i * 3 + 0] = scaledRgba[i * 4 + 0];
    dstRgb256[i * 3 + 1] = scaledRgba[i * 4 + 1];
    dstRgb256[i * 3 + 2] = scaledRgba[i * 4 + 2];
  }

  return true;
}
