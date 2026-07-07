#pragma once 
#include <EGL/egl.h> // EGLDisplay, EGLContext, EGLSurface
#include <EGL/eglext.h> // EGL extention declarations
#include <GLES3/gl3.h> // GLES 3 headers / structures
#include <GLES2/gl2ext.h> // imports GLES extention tags (GL_TEXTURE_EXTERNAL_OES)
#include <android/native_window.h>
#include <android/hardware_buffer.h>

// function pointer variables declared in eglext.h and glext.h. Look at what comes after 'get', that is the purpose of these function pointers.
extern PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC fn_eglGetNativeClientBufferANDROID; // the ANativeWindow object
extern PFNEGLCREATEIMAGEKHRPROC fn_eglCreateImageKHR; // creates EGLImage
extern PFNEGLDESTROYIMAGEKHRPROC fn_eglDestroyImageKHR; // destroy EGLImage when no longer needed / resaurce dealocated
extern PFNGLEGLIMAGETARGETTEXTURE2DOESPROC fn_glEGLImageTargetTexture2DOES; // the binding of EGLImage to the target -> GL_TEXTURE_EXTERNAL_OES

class EGLManager 
{
    public:
        EGLManager();
        ~EGLManager();

        bool InitializeEGL(ANativeWindow* window);
        void ReleaseEGL();
        GLuint InitGLExternalTexture();
        EGLImageKHR BindHardwareBuffer(AHardwareBuffer* buffer, GLuint textureId);
        void UnbindHardwareBuffer(EGLImageKHR image);
        void SwapBuffers();
    
    private:
        EGLDisplay display_;
        EGLConfig config_;
        EGLContext context_;
        EGLSurface surface_;
        ANativeWindow* nativeWindow_;
        bool extensionsLoaded_;

        bool loadExtentions();
};