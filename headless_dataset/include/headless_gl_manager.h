#pragma once
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1 
#endif

#include <EGL/egl.h>      // EGLDisplay, EGLContext, EGLSurface
#include <EGL/eglext.h>   // EGL extention declarations
#include <GL/gl.h>        // OpenGL function declarations
#include <GL/glext.h>     // Extention declarations
#include <vector>
#include <string>

struct Shaders {
    static constexpr const char* VERTEX_SOURCE = R"glsl(
        #version 330 core
        layout(location = 0) in vec4 a_position;
        layout(location = 1) in vec2 a_texCoords;
        out vec2 v_texCoords;
        void main() {
            gl_Position = vec4(a_position, 0.0, 1.0);
            v_texCoords = a_texCoords;
        }
    )glsl";

    static constexpr const char* FRAGMENT_SOURCE = R"glsl(
        #version 330 core
        in vec2 v_texCoords;
        out vec4 outColor;
        uniform sampler 2D u_texture;
        void main() {
            outColor = texture(u_texture, v_texCoords);
        }
    )glsl";

    static constexpr const char* STROKE_VERTEX_SOURCE = R"glsl(
        #version 330 core
        layout(location = 0) in vec2 a_displacedPosition;
        layout(location = 1) in vec2 a_originalTexCoords;
        out vec2 v_texCoords;
        void main() {
            gl_Position = vec4(a_displacedPosition, 0.0, 1.0);
            v_texCoords = a_originalTexCoords;
        }
    )glsl";

    static constexpr const char* STROKE_FRAGMENT_SOURCE = R"glsl(
        #version 330 core
        in vec2 v_texCoords;
        out vec4 outColor; 
        uniform sampler2D u_texture;
        void main() {
            outColor = texture(u_texture, v_texCoords);
        }
    )glsl";
};

class HeadlessGLManager 
{
    public:
        HeadlessGLManager();
        ~HeadlessGLManager();

        bool InitGLManager();

        void DrawOriginal(GLuint textureId);
        void DrawStrokeEffect(const std::vector<float>& meshVertexData, GLuint textureId);

        bool RenderWarpedImage(
            const uint8_t* rgbaInput,
            int width,
            int height,
            const std::vector<float>& meshVertexData,
            std::vector<uint8_t>& outWarped
        );

    private:
        EGLDisplay eglDisplay_ = EGL_NO_DISPLAY;
        EGLContext eglContext_ = EGL_NO_CONTEXT;
        EGLConfig eglConfig_ = nullptr;
        EGLSurface pBufferSurface_ = EGL_NO_SURFACE;

        GLuint fbo_ = 0;
        GLuint rboColor_ = 0;
        GLuint inputTexture_ = 0;

        GLuint programId_ = 0;
        GLuint vao_ = 0;
        GLuint vbo_ = 0;
        GLint textureUniformLocation_ = 0;

        GLuint strokeProgramId_ = 0;
        GLuint strokeVao_ = 0;
        GLuint strokeVbo_ = 0;
        GLuint strokeEbo_ = 0;
        GLint strokeTextureUniformLocation_ = -1;
        GLsizei numTriangleIndices_ = 0;

        bool initEGL();
        bool initShaders();
        bool initFBO(int fboWidth = 2048, int fboHeight = 2048);
        GLuint compileShader(GLenum type, const char* source);
};