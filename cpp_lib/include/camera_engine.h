#pragma once
#include <android/native_window.h>

class CameraEngine
{
public:
    explicit CameraEngine(ANativeWindow *window, int32_t width, int32_t height, int32_t format);
    ~CameraEngine();

    ANativeWindow *GetWindow() const { return window_; }
    int32_t GetWidth() const { return width_; }
    int32_t GetHeight() const { return height_; }
    int32_t GetFormat() const { return format_; }

private:
    ANativeWindow *window_;
    int32_t width_;
    int32_t height_;
    int32_t format_;
};