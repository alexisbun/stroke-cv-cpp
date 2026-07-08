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

        bool InitShaders();
        void DrawTexture(GLuint textureId);

    private:
        EGLDisplay display_;
        EGLConfig config_;
        EGLContext context_;
        EGLSurface surface_;
        ANativeWindow* nativeWindow_;
        bool extensionsLoaded_;

        bool loadExtentions();

        GLuint programId_ = 0;
        GLuint vao_ = 0;
        GLuint vbo_ = 0;
        GLint textureUniformLocation_ = -1;
};

struct Shaders
{
    static constexpr const char* VERTEX_SOURCE = R"glsl(
        #version 300 es
        layout(location = 0) in vec4 position;
        layout(location = 1) in vec2 texCoords;
        out vec2 v_texCoords;
        void main() {
            gl_Position = position;
            v_texCoords = texCoords;
        }
    )glsl";

    static constexpr const char* FRAGMENT_SOURCE = R"glsl(
        #version 300 es
        #extension GL_OES_EGL_image_external_essl3 : require
        precision mediump float;
        in vec2 v_texCoords;
        out vec4 outColor;
        uniform samplerExternalOES u_texture;
        void main() {
            outColor = texture(u_texture, v_texCoords);
        }
    )glsl";
}; 
// With mediapipe integration, change uniform 'samplerExternalOES u_texture' to 'uniform sampler2D u_texture'
// because mediapipe uses GL_TEXTURE_2D as its texture format.


