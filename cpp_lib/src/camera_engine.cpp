#include "camera_engine.h"
#include "ndk_camera.h"
#include "mediapipe_face_mesh.h"

#include <spdlog/spdlog.h>
#include <utils.h>

extern FaceMesh faceMesh;

CameraEngine::CameraEngine(ANativeWindow *window, int32_t width, int32_t height,
                           int32_t format)
    : ndkCamera_(nullptr), displayWindow_(window), width_(width),
      height_(height), format_(format), textureId_(0), isRunning_(false),
      frameReady_(false), pendingImage_(nullptr), pendingBuffer_(nullptr) {
  utils::init_native_logging();
  if (displayWindow_ != nullptr) {
    ANativeWindow_acquire(displayWindow_);
  }
  isRunning_ = true;
  renderThread_ = std::thread(&CameraEngine::renderLoop, this); // Spawns new independent thread for/ rendering to run OpenGL and EGL calls.
}

CameraEngine::~CameraEngine() {
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
  textureId_ =
      eglManager_
          .InitGLExternalTexture(); // Generate external texture and assign the
                                    // texture ID to textureId_.
  if (!readerHandler_.InitReader(
          width_, height_)) { // Instantiates AImageReader image buffer queue.
    spdlog::error("EGL Initialization failed!");
    eglManager_.ReleaseEGL();
    return;
  }
  spdlog::info("JNI: nativeAttach called. Surface address: {}",
               (void *)displayWindow_);
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
      EGLImageKHR image =
          eglManager_.BindHardwareBuffer(localBuffer, textureId_);
      if (image != EGL_NO_IMAGE_KHR) {
        eglManager_.DrawTexture(textureId_);

        std::vector<MpNormalizedLandmark> landmarks;
        if (faceMesh.GetLatestLandmarks(landmarks)) {
           std::vector<float> projectedCoordinates;
           projectedCoordinates.reserve(landmarks.size() * 2);

           for (const auto& lm : landmarks) {
                float ndcX = (lm.x * 2.0f) - 1.0f;
                float ndcY = -((lm.y * 2.0f) - 1.0f);
                
                projectedCoordinates.push_back(ndcX);
                projectedCoordinates.push_back(ndcY);
            }
            eglManager_.DrawLandmarks(projectedCoordinates);
        }

        // present comined camera frame
        eglManager_.SwapBuffers();
        auto now = std::chrono::high_resolution_clock::now();

        // INTRODUCE A WAY OF DOING A TOGGLE FOR THIS
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

          // smoothed FPS.
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
