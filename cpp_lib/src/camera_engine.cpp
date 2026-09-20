// TEST EXPERIMENTAL 2

#include "camera_engine.h"
#include "ndk_camera.h"
#include "mediapipe_face_mesh.h"
#include "mobile_unet.h" 

#include <spdlog/spdlog.h>
#include <utils.h>

extern FaceMesh faceMesh;
extern StrokeModelInference strokeModelInference;
extern MobileUNet mobileUNetInference;

CameraEngine::CameraEngine(ANativeWindow *window, int32_t width, int32_t height,
                           int32_t format)
    : ndkCamera_(nullptr), displayWindow_(window), width_(width),
      height_(height), format_(format), textureId_(0), isRunning_(false),
      frameReady_(false), pendingImage_(nullptr), pendingBuffer_(nullptr) {
  utils::init_native_logging();
  if (displayWindow_ != nullptr) {
    ANativeWindow_acquire(displayWindow_);
  }
  spdlog::info("JNI: nativeAttach called. Surface address: {}", (void *)displayWindow_);
  isRunning_ = true;
  unetRunning_ = true;
  unetWorkerThread_ = std::thread(&CameraEngine::unetWorkerLoop, this);
  renderThread_ = std::thread(&CameraEngine::renderLoop, this); // Spawns new independent thread for/ rendering to run OpenGL and EGL calls.
}

CameraEngine::~CameraEngine() {
  {
    std::lock_guard<std::mutex> lock(unetMutex_);
    unetRunning_ = false;
    if (unetPendingBuffer_ != nullptr) {
      AHardwareBuffer_release(unetPendingBuffer_);
      unetPendingBuffer_ = nullptr;
    }
  }
  unetCv_.notify_all();
  if (unetWorkerThread_.joinable()) {
    unetWorkerThread_.join();
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    isRunning_ = false;
    if (pendingImage_ != nullptr) {
      AImage_delete(pendingImage_);
      pendingImage_ = nullptr;
    }
  } // unlock mutex before thread joins
  cv_.notify_all();
  if (renderThread_.joinable()) {
    renderThread_.join();
  }

  if (displayWindow_ != nullptr) {
    ANativeWindow_release(displayWindow_);
    displayWindow_ = nullptr;
  }
}

void CameraEngine::renderLoop() {
  spdlog::info("Render thread started.");
  if (!eglManager_.InitializeEGL(displayWindow_)) { // Boot EGL display/context
                                                    // on the rendering thread.
    return;                                         // early return if it fails
  }
  textureId_ = eglManager_.InitGLExternalTexture(); // Generate external texture and assign the texture ID to textureId_.
  if (!readerHandler_.InitReader(width_, height_)) { // Instantiates AImageReader image buffer queue.
    spdlog::error("EGL Initialization failed!");
    eglManager_.ReleaseEGL();
    return;
  }
  AImageReader_ImageListener listener;
  listener.context = this;
  listener.onImageAvailable = [](void *context, AImageReader *reader) {
    auto *engine = reinterpret_cast<CameraEngine *>(context);
    engine->onFrameAvailable(reader);
  };
  readerHandler_.SetImageListener(
      &listener); // Register listeners to recive new frames.
  ndkCamera_ = new NDKCamera();
  ndkCamera_->EnumerateCamera();
  ndkCamera_->CreateSession(readerHandler_.GetWindow());
  ndkCamera_->StartPreview(true);
  while (true) {
    AImage *localImage = nullptr;
    AHardwareBuffer *localBuffer = nullptr;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return !isRunning_ || frameReady_; });
      if (!isRunning_) {
        break;
      }

      localImage = pendingImage_;
      localBuffer = pendingBuffer_;
      pendingImage_ = nullptr;
      pendingBuffer_ = nullptr;
      frameReady_ = false;
    }
    if (localImage != nullptr && localBuffer != nullptr) {
      EGLImageKHR image = eglManager_.BindHardwareBuffer(localBuffer, textureId_);
      if (image != EGL_NO_IMAGE_KHR) {
        eglManager_.DrawTexture(textureId_);

        if (hasNewDelta_.exchange(false)) {
          std::lock_guard<std::mutex> lock(unetResultMutex_);
          eglManager_.UploadDeltaTexture(completedDeltaRgb_.data());
          activeDeltaRoi_ = completedRoi_;
          deltaReady_ = true;
        }

        std::vector<MpNormalizedLandmark> landmarks;
        FaceROI currentRoi{};

        if (faceMesh.GetLatestLandmarks(landmarks)) {
          currentRoi = ComputeFaceROI(landmarks, width_, height_);

          std::vector<MpNormalizedLandmark> strokeLandmarks;
          if (strokeModelInference.PredictStrokeLandmarks(landmarks, strokeLandmarks)) {
            spdlog::debug("PredictStrokeLandmarks called!");
            std::vector<float> meshVertexData;
            meshVertexData.reserve(478 * 4);
            
            size_t i = 0;
            for (const auto& lm : landmarks) {
              const auto& stroke = strokeLandmarks[i++];

              meshVertexData.push_back(1.0f - (stroke.y * 2.0f)); // stroke.x
              meshVertexData.push_back((stroke.x * 2.0f) - 1.0f); // stroke.y
              meshVertexData.push_back(lm.x);
              meshVertexData.push_back(lm.y);
            }

            if (currentRoi.size > 0) {
              FaceROI roiToUse = deltaReady_ ? activeDeltaRoi_ : currentRoi;
              float roiMinX = static_cast<float>(roiToUse.x) / static_cast<float>(width_);
              float roiMinY = static_cast<float>(roiToUse.y) / static_cast<float>(height_);
              float roiSizeX = static_cast<float>(roiToUse.size) / static_cast<float>(width_);
              float roiSizeY = static_cast<float>(roiToUse.size) / static_cast<float>(height_);

              float mouthX = landmarks.size() > 291 ? landmarks[291].x : 0.5f;
              float mouthY = landmarks.size() > 291 ? landmarks[291].y : 0.5f;
              float mouthRoiX = (mouthX - roiMinX) / roiSizeX;
              float mouthRoiY = (mouthY - roiMinY) / roiSizeY;

              eglManager_.DrawStrokeEffect(
                meshVertexData, 
                textureId_,
                roiMinX, roiMinY, roiSizeX, roiSizeY,
                mouthRoiX, mouthRoiY,
                true);
            }

            if (currentRoi.size > 0 && mobileUNetInference.IsInitialized()) {
              std::unique_lock<std::mutex> lock(unetMutex_, std::try_to_lock);
              if (lock.owns_lock() && !unetRequestReady_) {
                AHardwareBuffer_acquire(localBuffer);
                unetPendingBuffer_ = localBuffer;
                unetPendingRoi_ = currentRoi;
                unetPendingOrigLm_ = landmarks;
                unetPendingStrokeLm_ = strokeLandmarks;
                unetRequestReady_ = true;
                unetCv_.notify_one();
              }
            }
          }    
        }

        // present comined camera frame
        eglManager_.SwapBuffers();
        auto now = std::chrono::high_resolution_clock::now();

        int64_t timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch()).count();
          
        faceMesh.ProcessFrame(localBuffer, timestamp_ms);

        if (isFirstFrame_) {
          isFirstFrame_ = false;
          lastWriteTime_ = now;
        } else {
          std::chrono::duration<double> elapsed = now - lastWriteTime_;
          lastWriteTime_ = now;
          double deltaSeconds = elapsed.count();
          
          if (deltaSeconds > 0.0) {
            double instantaneousFps = 1.0 / deltaSeconds;
            double alpha = 0.05;
            currentFps_ = (alpha * instantaneousFps) + ((1.0 - alpha) * currentFps_);
          }
        }
      }
      AImage_delete(localImage);
    }
  }
  if (ndkCamera_ != nullptr) {
    ndkCamera_->StartPreview(false);
    delete ndkCamera_;
    ndkCamera_ = nullptr;
  }
  readerHandler_.ReleaseReader();
  eglManager_.ReleaseEGL();
}

void CameraEngine::onFrameAvailable(AImageReader *reader) {
  AImage *image = nullptr;
  media_status_t status = AImageReader_acquireLatestImage(reader, &image);
  if (status != AMEDIA_OK || image == nullptr) {
    return;
  }
  AHardwareBuffer *buffer = nullptr;
  status = AImage_getHardwareBuffer(image, &buffer);
  if (status != AMEDIA_OK || buffer == nullptr) {
    AImage_delete(image);
    return;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning_) {
      AImage_delete(image);
      return;
    }
    if (pendingImage_ != nullptr) {
      AImage_delete(pendingImage_);
    }
    pendingImage_ = image;
    pendingBuffer_ = buffer;
    frameReady_ = true;
  }
  cv_.notify_one();
}

void CameraEngine::unetWorkerLoop() {
  while (true) {
    AHardwareBuffer *buffer = nullptr;
    FaceROI roi{};
    std::vector<MpNormalizedLandmark> origLm;
    std::vector<MpNormalizedLandmark> strokeLm;

    {
      std::unique_lock<std::mutex> lock(unetMutex_);
      unetCv_.wait(lock, [this] { return !unetRunning_ || unetRequestReady_; });
      if (!unetRunning_) {
        break;
      }
      buffer = unetPendingBuffer_;
      roi = unetPendingRoi_;
      origLm = std::move(unetPendingOrigLm_);
      strokeLm = std::move(unetPendingStrokeLm_);
      unetPendingBuffer_ = nullptr;
      unetRequestReady_ = false;
    }

    if (buffer != nullptr && roi.size > 0) {
      std::vector<uint8_t> cropOrig(256 * 256 * 3);
      if (CropAndResizeHardwareBuffer(buffer, roi, cropOrig.data())) {
        AHardwareBuffer_release(buffer);

        std::vector<uint8_t> cropWarp(256 * 256 * 3);
        WarpCropTriangles(cropOrig.data(), origLm, strokeLm, roi, width_, height_, cropWarp.data());

        alignas(64) static std::array<float, INPUT_SIZE> input;
        alignas(64) static std::array<float, OUTPUT_SIZE> delta;
        constexpr int PIXELS = 256 * 256;

        for (int i = 0; i < PIXELS; ++i) {
          input[0 * PIXELS + i] = cropOrig[i * 3 + 0] / 255.0f;
          input[1 * PIXELS + i] = cropOrig[i * 3 + 1] / 255.0f;
          input[2 * PIXELS + i] = cropOrig[i * 3 + 2] / 255.0f;

          input[3 * PIXELS + i] = cropWarp[i * 3 + 0] / 255.0f;
          input[4 * PIXELS + i] = cropWarp[i * 3 + 1] / 255.0f;
          input[5 * PIXELS + i] = cropWarp[i * 3 + 2] / 255.0f;
        }

        if (mobileUNetInference.UNetInference(input, delta)) {
          std::lock_guard<std::mutex> lock(unetResultMutex_);
          PlanarToRBB(delta.data(), completedDeltaRgb_.data());
          completedRoi_ = roi;
          hasNewDelta_.store(true);
        }
      } else {
        AHardwareBuffer_release(buffer);
      }
    }
  }
}
