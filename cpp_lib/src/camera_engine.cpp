#include "camera_engine.h"
#include "ndk_camera.h"

#include <android/log.h>
#define LOG_TAG "CameraMVP_Engine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

CameraEngine::CameraEngine(ANativeWindow *window, int32_t width, int32_t height, int32_t format)
    : ndkCamera_(nullptr), 
      displayWindow_(window),
      width_(width), 
      height_(height), 
      format_(format),
      textureId_(0),
      isRunning_(false),
      frameReady_(false),
      pendingImage_(nullptr),
      pendingBuffer_(nullptr)
{
    if (displayWindow_ != nullptr) {
        ANativeWindow_acquire(displayWindow_);
    }
    isRunning_ = true;
    renderThread_ = std::thread(&CameraEngine::renderLoop, this); // Spawns new independent thread for rendering to run OpenGL and EGL calls.
}

CameraEngine::~CameraEngine()
{
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

void CameraEngine::renderLoop() 
{

    LOGI("Render thread started.");
    if (!eglManager_.InitializeEGL(displayWindow_)) {
        LOGE("EGL Initialization failed!");  // Boot EGL display/context on the rendering thread.
        return; // early return if it fails
    }
    LOGI("EGL Initialized successfully.");
    textureId_ = eglManager_.InitGLExternalTexture(); // Generate external texture and assign the texture ID to textureId_.
    if (!readerHandler_.InitReader(width_, height_)) { // Instantiates AImageReader image buffer queue.
        LOGE("ImageReader Initialization failed!");
        eglManager_.ReleaseEGL();
        return;
    }
    LOGI("ImageReader Initialized successfully.");
    AImageReader_ImageListener listener;
    listener.context = this;
    listener.onImageAvailable = [](void* context, AImageReader* reader) {
        auto* engine = reinterpret_cast<CameraEngine*>(context);
        engine->onFrameAvailable(reader);
    };
    readerHandler_.SetImageListener(&listener); // Register listeners to recive new frames.
    ndkCamera_ = new NDKCamera(); 
    ndkCamera_->EnumerateCamera();
    ndkCamera_->CreateSession(readerHandler_.GetWindow());
    ndkCamera_->StartPreview(true);
    while (true) {
        AImage* localImage = nullptr;
        AHardwareBuffer* localBuffer = nullptr;
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
                eglManager_.SwapBuffers();
                eglManager_.UnbindHardwareBuffer(image);
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

void CameraEngine::onFrameAvailable(AImageReader* reader) 
{
    AImage* image = nullptr;
    media_status_t status = AImageReader_acquireLatestImage(reader, &image);
    if (status != AMEDIA_OK || image == nullptr) { 
        return;
    }
    AHardwareBuffer* buffer = nullptr;
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
