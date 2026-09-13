#pragma once

#include "face_landmarker.h"
#include "face_mesh_triangles.h"
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

inline void WarpCropTriangles(
    const uint8_t* srcCropRgb,
    const std::vector<MpNormalizedLandmark>& origLm,
    const std::vector<MpNormalizedLandmark>& strokeLm,
    const FaceROI& roi,
    int imgW, int imgH,
    uint8_t* dstWarpRgb)
{
    if (!srcCropRgb || !dstWarpRgb || origLm.size() < 478 || strokeLm.size() < 478 || roi.size <= 0) {
        if (srcCropRgb && dstWarpRgb) {
            std::memcpy(dstWarpRgb, srcCropRgb, 256 * 256 * 3);
        }
        return;
    }

    std::memcpy(dstWarpRgb, srcCropRgb, 256 * 256 * 3);

    struct Point2D { float x; float y; };
    std::vector<Point2D> origPts(478);
    std::vector<Point2D> strokePts(478);

    float invRoiSize = 255.0f / static_cast<float>(roi.size);
    for (size_t i = 0; i < 478; ++i) {
        origPts[i].x = (origLm[i].x * static_cast<float>(imgW) - static_cast<float>(roi.x)) * invRoiSize;
        origPts[i].y = (origLm[i].y * static_cast<float>(imgH) - static_cast<float>(roi.y)) * invRoiSize;

        strokePts[i].x = (strokeLm[i].x * static_cast<float>(imgW) - static_cast<float>(roi.x)) * invRoiSize;
        strokePts[i].y = (strokeLm[i].y * static_cast<float>(imgH) - static_cast<float>(roi.y)) * invRoiSize;
    }

    constexpr size_t numTriangles = NUM_FACE_INDICES / 3;
    for (size_t t = 0; t < numTriangles; ++t) {
        uint16_t i0 = FACE_MESH_TRIANGLES[t * 3 + 0];
        uint16_t i1 = FACE_MESH_TRIANGLES[t * 3 + 1];
        uint16_t i2 = FACE_MESH_TRIANGLES[t * 3 + 2];

        if (i0 >= 478 || i1 >= 478 || i2 >= 478) continue;

        const Point2D& p0 = strokePts[i0];
        const Point2D& p1 = strokePts[i1];
        const Point2D& p2 = strokePts[i2];

        const Point2D& q0 = origPts[i0];
        const Point2D& q1 = origPts[i1];
        const Point2D& q2 = origPts[i2];

        int minX = std::max(0, static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))));
        int maxX = std::min(255, static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))));
        int minY = std::max(0, static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y}))));
        int maxY = std::min(255, static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y}))));

        if (minX > maxX || minY > maxY) continue;

        float den = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
        if (std::abs(den) < 1e-5f) continue;
        float invDen = 1.0f / den;

        for (int y = minY; y <= maxY; ++y) {
            float py = static_cast<float>(y) + 0.5f;
            for (int x = minX; x <= maxX; ++x) {
                float px = static_cast<float>(x) + 0.5f;

                float l0 = ((p1.y - p2.y) * (px - p2.x) + (p2.x - p1.x) * (py - p2.y)) * invDen;
                float l1 = ((p2.y - p0.y) * (px - p2.x) + (p0.x - p2.x) * (py - p2.y)) * invDen;
                float l2 = 1.0f - l0 - l1;

                if (l0 >= -1e-3f && l1 >= -1e-3f && l2 >= -1e-3f) {
                    float sx = l0 * q0.x + l1 * q1.x + l2 * q2.x;
                    float sy = l0 * q0.y + l1 * q1.y + l2 * q2.y;

                    int x0 = std::clamp(static_cast<int>(std::floor(sx)), 0, 254);
                    int y0 = std::clamp(static_cast<int>(std::floor(sy)), 0, 254);
                    float fx = std::clamp(sx - static_cast<float>(x0), 0.0f, 1.0f);
                    float fy = std::clamp(sy - static_cast<float>(y0), 0.0f, 1.0f);

                    float w00 = (1.0f - fx) * (1.0f - fy);
                    float w10 = fx * (1.0f - fy);
                    float w01 = (1.0f - fx) * fy;
                    float w11 = fx * fy;

                    int dstIdx = (y * 256 + x) * 3;
                    for (int c = 0; c < 3; ++c) {
                        float sampleVal = w00 * srcCropRgb[(y0 * 256 + x0) * 3 + c] +
                                          w10 * srcCropRgb[(y0 * 256 + (x0 + 1)) * 3 + c] +
                                          w01 * srcCropRgb[((y0 + 1) * 256 + x0) * 3 + c] +
                                          w11 * srcCropRgb[((y0 + 1) * 256 + (x0 + 1)) * 3 + c];
                        dstWarpRgb[dstIdx + c] = static_cast<uint8_t>(std::clamp(sampleVal, 0.0f, 255.0f));
                    }
                }
            }
        }
    }
}
