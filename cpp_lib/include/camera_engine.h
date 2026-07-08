#pragma once
#include <android/native_window.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>

#include <thread>
#include <mutex>
#include <condition_variable>

#include "egl_utils.h"
#include "image_reader_handler.h"

class NDKCamera;

class CameraEngine
{
public:
    explicit CameraEngine(ANativeWindow *window, int32_t width, int32_t height, int32_t format);
    ~CameraEngine();

    ANativeWindow *GetWindow() const { return displayWindow_; }
    int32_t GetWidth() const { return width_; }
    int32_t GetHeight() const { return height_; }
    int32_t GetFormat() const { return format_; }

private:
    NDKCamera* ndkCamera_;
    EGLManager eglManager_;
    ImageReaderHandler readerHandler_;

    ANativeWindow *displayWindow_;
    int32_t width_;
    int32_t height_;
    int32_t format_;
    GLuint textureId_;

    std::thread renderThread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    
    bool isRunning_;
    bool frameReady_;

    AImage* pendingImage_;
    AHardwareBuffer* pendingBuffer_;

    void renderLoop();
    void onFrameAvailable(AImageReader* reader);
};