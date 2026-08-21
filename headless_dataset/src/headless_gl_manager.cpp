#include "headless_gl_manager.h"
#include "face_mesh_triangles.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstddef>

HeadlessGLManager::HeadlessGLManager() 
{
}

HeadlessGLManager::~HeadlessGLManager()
{
    if (fbo_ != 0) {
        glDeleteFramebuffers(1, &fbo_);
        glDeleteRenderbuffers(1, &rboColor_);
        fbo_ = 0;
        rboColor_ = 0;
    }
    if (inputTexture_ != 0) {
        glDeleteTextures(1, &inputTexture_);
        inputTexture_ = 0;
    }
    if (programId_ != 0) {
        glDeleteProgram(programId_);
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_);
        programId_ = 0;
    }
    if (strokeProgramId_ != 0) {
        glDeleteProgram(strokeProgramId_);
        glDeleteVertexArrays(1, &strokeVao_);
        glDeleteBuffers(1, &strokeVbo_);
        glDeleteBuffers(1, &strokeEbo_);
        strokeProgramId_ = 0;
    }
    if (eglDisplay_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(eglDisplay_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (pBufferSurface_ != EGL_NO_SURFACE) {
            eglDestroySurface(eglDisplay_, pBufferSurface_);
            pBufferSurface_ = EGL_NO_SURFACE;
        }
        if (eglContext_ != EGL_NO_CONTEXT) {
            eglDestroyContext(eglDisplay_, eglContext_);
            eglContext_ = EGL_NO_CONTEXT;
        }
        eglTerminate(eglDisplay_);
        eglDisplay_ = EGL_NO_DISPLAY;
    }
}

bool HeadlessGLManager::InitGLManager() 
{
    if (!initEGL()) {
        spdlog::info("EGL context initialization failed.");
        return false;
    }
    if (!initShaders()) {
        spdlog::info("Failed to compile shaders.");
        return false;
    }
    if (!initFBO()) {
        spdlog::info("Failed to create FBO.");
        return false;
    }
    return true;
}

bool HeadlessGLManager::initEGL() {
    eglDisplay_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    
    EGLint major, minor;
    if (!eglInitialize(eglDisplay_, &major, &minor)) {
        return false;
    }
    
    EGLint configAttribs[] = {
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(eglDisplay_, configAttribs, &eglConfig_, 1, &numConfigs) || numConfigs < 1) {
        return false;
    }

    eglBindAPI(EGL_OPENGL_API);

    EGLint contextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };

    eglContext_ = eglCreateContext(eglDisplay_, eglConfig_, EGL_NO_CONTEXT, contextAttribs);
    if (eglContext_ == EGL_NO_CONTEXT) {
        return false;
    }

    EGLint pBufferAttribs[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
    pBufferSurface_ = eglCreatePbufferSurface(eglDisplay_, eglConfig_, pBufferAttribs);

    if (pBufferSurface_ == EGL_NO_SURFACE) {
        return false;
    }

    if (!eglMakeCurrent(eglDisplay_, pBufferSurface_, pBufferSurface_, eglContext_)) {
        return false;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    return true;
}

GLuint HeadlessGLManager::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glDeleteShader(shader);
    }
    return shader;
}

bool HeadlessGLManager::initShaders() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, Shaders::VERTEX_SOURCE);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, Shaders::FRAGMENT_SOURCE);

    programId_ = glCreateProgram();
    glAttachShader(programId_, vertexShader);
    glAttachShader(programId_, fragmentShader);
    glLinkProgram(programId_);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    textureUniformLocation_ = glGetUniformLocation(programId_, "u_texture");

    float quadVertices[] = {
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 0.0f,
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    GLuint strokeVertexShader = compileShader(GL_VERTEX_SHADER, Shaders::STROKE_VERTEX_SOURCE);
    GLuint strokeFragmentShader = compileShader(GL_FRAGMENT_SHADER, Shaders::STROKE_FRAGMENT_SOURCE);

    strokeProgramId_ = glCreateProgram();
    glAttachShader(strokeProgramId_, strokeVertexShader);
    glAttachShader(strokeProgramId_, strokeFragmentShader);
    glLinkProgram(strokeProgramId_);
    glDeleteShader(strokeVertexShader);
    glDeleteShader(strokeFragmentShader);

    strokeTextureUniformLocation_ = glGetUniformLocation(strokeProgramId_, "u_texture");
    numTriangleIndices_ = static_cast<GLsizei>(NUM_FACE_INDICES);

    glGenVertexArrays(1, &strokeVao_);
    glGenBuffers(1, &strokeVbo_);
    glGenBuffers(1, &strokeEbo_);
    glBindVertexArray(strokeVao_);

    glBindBuffer(GL_ARRAY_BUFFER, strokeVbo_);
    glBufferData(GL_ARRAY_BUFFER, 478 * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, strokeEbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(FACE_MESH_TRIANGLES), FACE_MESH_TRIANGLES, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    glGenTextures(1, &inputTexture_);
    glBindTexture(GL_TEXTURE_2D, inputTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

bool HeadlessGLManager::initFBO(int maxWidth, int maxHeight) 
{
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenRenderbuffers(1, &rboColor_);
    glBindRenderbuffer(GL_RENDERBUFFER, rboColor_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, maxWidth, maxHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rboColor_);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void HeadlessGLManager::DrawOriginal(GLuint textureId) {
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(programId_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glUniform1i(textureUniformLocation_, 0);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void HeadlessGLManager::DrawStrokeEffect(const std::vector<float>& meshVertexData, GLuint textureId) {
    if (meshVertexData.empty()) return;
    glUseProgram(strokeProgramId_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glUniform1i(strokeTextureUniformLocation_, 0);
    glBindVertexArray(strokeVao_);
    glBindBuffer(GL_ARRAY_BUFFER, strokeVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, meshVertexData.size() * sizeof(float), meshVertexData.data());
    
    glDrawElements(GL_TRIANGLES, numTriangleIndices_, GL_UNSIGNED_SHORT, 0);
    glBindVertexArray(0);
}

bool HeadlessGLManager::RenderWarpedImage(
    const uint8_t* rgbaInput, 
    int width, 
    int height,
    const std::vector<float>& meshVertexData,
    std::vector<uint8_t>& outWarped
)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width, height);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaInput);

    DrawOriginal(inputTexture_);
    DrawStrokeEffect(meshVertexData, inputTexture_);

    outWarped.resize(width * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, outWarped.data());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

