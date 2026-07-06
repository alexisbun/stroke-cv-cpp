#include "camera_engine.h"

CameraEngine::CameraEngine(ANativeWindow *window, int32_t width, int32_t height, int32_t format)
    : window_(window), width_(width), height_(height), format_(format)
{
}

CameraEngine::~CameraEngine()
{
    if (window_ != nullptr)
    {
        ANativeWindow_release(window_);
        window_ = nullptr;
    }
}
