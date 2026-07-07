#include "egl_utils.h"

PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC fn_eglGetNativeClientBufferANDROID = nullptr;
PFNEGLCREATEIMAGEKHRPROC fn_eglCreateImageKHR = nullptr;
PFNEGLDESTROYIMAGEKHRPROC fn_eglDestroyImageKHR = nullptr;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC fn_glEGLImageTargetTexture2DOES = nullptr;

EGLManager::EGLManager()
    : display_(EGL_NO_DISPLAY),
      config_(nullptr),
      context_(EGL_NO_CONTEXT),
      surface_(EGL_NO_SURFACE),
      nativeWindow_(nullptr),
      extensionsLoaded_(false)
{
}

EGLManager::~EGLManager()
{
}

bool EGLManager::InitializeEGL(ANativeWindow* window)
{
    return false;
}

void EGLManager::ReleaseEGL()
{
}

GLuint EGLManager::InitGLExternalTexture()
{
    return 0;
}

EGLImageKHR EGLManager::BindHardwareBuffer(AHardwareBuffer* buffer, GLuint textureId)
{
    return EGL_NO_IMAGE_KHR; // stub for no image
}

void EGLManager::UnbindHardwareBuffer(EGLImageKHR image)
{
}

void EGLManager::SwapBuffers()
{
}

bool EGLManager::loadExtentions()
{
    return false;
}
