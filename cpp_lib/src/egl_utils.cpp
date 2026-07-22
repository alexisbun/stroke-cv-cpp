#include "egl_utils.h"
#include "mediapipe_face_mesh.h"
#include <unordered_map>
#include <vector>

PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC fn_eglGetNativeClientBufferANDROID = nullptr;
PFNEGLCREATEIMAGEKHRPROC fn_eglCreateImageKHR = nullptr;
PFNEGLDESTROYIMAGEKHRPROC fn_eglDestroyImageKHR = nullptr;
PFNGLEGLIMAGETARGETTEXTURE2DOESPROC fn_glEGLImageTargetTexture2DOES = nullptr;

// initilize all EGL state variables to default or null values
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
    ReleaseEGL();
}

// initilize EGL state variables and map to surface obtainted from Flutter.
bool EGLManager::InitializeEGL(ANativeWindow* window)
{
    if (window == nullptr) {
        return false;
    }

    nativeWindow_ = window;
    ANativeWindow_acquire(nativeWindow_);

    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY); // standard system graphics display driver
    if (display_ == EGL_NO_DISPLAY) {
        return false;
    }

    EGLint major, minor;
    if (!eglInitialize(display_, &major, &minor)) { // initialize egl interface on the display
        return false;
    }

    const EGLint attributeList[] = { // frame buffer formating specification attributes
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8, 
        EGL_ALPHA_SIZE, 8, 
        EGL_DEPTH_SIZE, 0,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(display_, attributeList, &config_, 1, &numConfigs) || numConfigs < 1) {
        return false;
    }

    const EGLint contextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, contextAttributes);
    if (context_ == EGL_NO_CONTEXT) {
        return false;
    }

    surface_ = eglCreateWindowSurface(display_, config_, nativeWindow_, nullptr); // 
    if (surface_ == EGL_NO_SURFACE) {
        return false;
    }

    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        return false;
    }

    if(!loadExtentions()) {
        return false;
    }

    if (!InitShaders()) {
        return false;
    }

    return true; // success, EGL initilized.
}

void EGLManager::ReleaseEGL()
{
    for (auto const& [buffer, image]: eglImageCache_) {
        if (image != EGL_NO_IMAGE_KHR) {
            fn_eglDestroyImageKHR(display_, image);
        }
    }
    eglImageCache_.clear();

    if (display_ != EGL_NO_DISPLAY && context_ != EGL_NO_CONTEXT) {
        eglMakeCurrent(display_, surface_, surface_, context_);
        
        if (programId_ != 0) {
            glDeleteProgram(programId_);
            programId_ = 0;
        }
        if (vao_ != 0) {
            glDeleteVertexArrays(1, &vao_);
            vao_ = 0;
        }
        if (vbo_ != 0) {
            glDeleteBuffers(1, &vbo_);
            vbo_ = 0;
        }
                if (pointProgramId_ != 0) {
            glDeleteProgram(pointProgramId_);
            pointProgramId_ = 0;
        }
        if (landmarkVao_ != 0) {
            glDeleteVertexArrays(1, &landmarkVao_);
            landmarkVao_ = 0;
        }
        if (landmarkVbo_ != 0) {
            glDeleteBuffers(1, &landmarkVbo_);
            landmarkVbo_ = 0;
        }
        if (landmarkEbo_ != 0) {
            glDeleteBuffers(1, &landmarkEbo_);
            landmarkEbo_ = 0;
        }
        if (meshProgramId_ != 0) {
            glDeleteProgram(meshProgramId_);
            meshProgramId_ = 0;
        }
    }

    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(display_, surface_);
        eglDestroyContext(display_, context_);
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }

    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(display_, surface_);
        eglDestroyContext(display_, context_);
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }

    if (nativeWindow_ != nullptr) {
        ANativeWindow_release(nativeWindow_);
        nativeWindow_ = nullptr;
    }

    surface_ = EGL_NO_SURFACE;
    context_ = EGL_NO_CONTEXT;
    config_ = nullptr;
    extensionsLoaded_ = false;
}

GLuint EGLManager::InitGLExternalTexture()
{
    GLuint textureId = 0; 

    glGenTextures(1, &textureId);                      // generate name/ID for texture object
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId); // bind external texture to current texture object

    // configure texture properties / sampling parameters (mandatory fields)
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return textureId;
}

// Connect AHardwareBuffer (Android) to GL_TEXTURE_EXTERNAL_OES (OpenGL)
EGLImageKHR EGLManager::BindHardwareBuffer(AHardwareBuffer* buffer, GLuint textureId)
{
    if (!extensionsLoaded_ || buffer == nullptr) {
        return EGL_NO_IMAGE_KHR;
    }

    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    auto it = eglImageCache_.find(buffer);

    if (it != eglImageCache_.end()) {
        // cache hit: reuse EGLImage already associated with this buffer
        image = it->second;
    } else {
        // cache miss: create EGLImage wrapper for this buffer
        EGLClientBuffer clientBuffer = fn_eglGetNativeClientBufferANDROID(buffer);
        if (clientBuffer == nullptr) {
            return EGL_NO_IMAGE_KHR;
        }
        EGLint attributes[] = {EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE};
        image = fn_eglCreateImageKHR(
            display_,
            EGL_NO_CONTEXT,
            EGL_NATIVE_BUFFER_ANDROID,
            clientBuffer,
            attributes
        );
        if (image != EGL_NO_IMAGE_KHR) {
            eglImageCache_[buffer] = image; // insert the EGLImage into cache.
        }
    }

    if (image != EGL_NO_IMAGE_KHR) {
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
        fn_glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, image);
    }

    return image; // returns EGLImageKHR handle
}

void EGLManager::SwapBuffers()
{
    if (display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE) {
        eglSwapBuffers(display_, surface_);
    }
}

bool EGLManager::loadExtentions()
{
    fn_eglGetNativeClientBufferANDROID = reinterpret_cast<PFNEGLGETNATIVECLIENTBUFFERANDROIDPROC>(
        eglGetProcAddress("eglGetNativeClientBufferANDROID")
    );
    
    fn_eglCreateImageKHR = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(
        eglGetProcAddress("eglCreateImageKHR")
    );
    
    fn_eglDestroyImageKHR = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(
        eglGetProcAddress("eglDestroyImageKHR")
    );
    
    fn_glEGLImageTargetTexture2DOES = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
        eglGetProcAddress("glEGLImageTargetTexture2DOES")
    );

    extensionsLoaded_ = (fn_eglGetNativeClientBufferANDROID != nullptr) &&
                        (fn_eglCreateImageKHR != nullptr) &&
                        (fn_eglDestroyImageKHR != nullptr) &&
                        (fn_glEGLImageTargetTexture2DOES != nullptr);

    return extensionsLoaded_;
}

bool EGLManager::InitShaders() {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &Shaders::VERTEX_SOURCE, nullptr);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &Shaders::FRAGMENT_SOURCE, nullptr);
    glCompileShader(fragmentShader);

    programId_ = glCreateProgram();
    glAttachShader(programId_, vertexShader);
    glAttachShader(programId_, fragmentShader);
    glLinkProgram(programId_);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    textureUniformLocation_ = glGetUniformLocation(programId_, "u_texture");
    float quadVertices[] = {
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  0.0f, 0.0f,
        -1.0f,  1.0f,  1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 0.0f,
    };
/*
    GLuint pointVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(pointVertexShader, 1, &Shaders::POINT_VERTEX_SOURCE, nullptr);
    glCompileShader(pointVertexShader);

    GLuint pointFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(pointFragmentShader, 1, &Shaders::POINT_FRAGMENT_SOURCE, nullptr);
    glCompileShader(pointFragmentShader);

    pointProgramId_ = glCreateProgram();
    glAttachShader(pointProgramId_, pointVertexShader);
    glAttachShader(pointProgramId_, pointFragmentShader);
    glLinkProgram(pointProgramId_);
    glDeleteShader(pointVertexShader);
    glDeleteShader(pointFragmentShader);
*/
    GLuint meshVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(meshVertexShader, 1, &Shaders::MESH_VERTEX_SOURCE, nullptr);
    glCompileShader(meshVertexShader);

    GLuint meshFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(meshFragmentShader, 1, &Shaders::MESH_FRAGMENT_SOURCE, nullptr);
    glCompileShader(meshFragmentShader);

    meshProgramId_ = glCreateProgram();
    glAttachShader(meshProgramId_, meshVertexShader);
    glAttachShader(meshProgramId_, meshFragmentShader);
    glLinkProgram(meshProgramId_);
    glDeleteShader(meshVertexShader);
    glDeleteShader(meshFragmentShader);

    meshTextureUniformLocation_ = glGetUniformLocation(meshProgramId_, "u_texture");
    //meshGradeUniformLocation_     = glGetUniformLocation(meshProgramId_, "u_clinicalGrade");
    meshDirectionUniformLocation_ = glGetUniformLocation(meshProgramId_, "u_droopDirection");  
    
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
    glGenVertexArrays(1, &landmarkVao_);
    glGenBuffers(1, &landmarkVbo_);
    glGenBuffers(1, &landmarkEbo_); // generates EBO buffer
    glBindVertexArray(landmarkVao_);

    glBindBuffer(GL_ARRAY_BUFFER, landmarkVbo_);
    glBufferData(GL_ARRAY_BUFFER, 478 * 5 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(4 * sizeof(float)));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, landmarkEbo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(FACE_MESH_TRIANGLES), FACE_MESH_TRIANGLES, GL_STATIC_DRAW);

    glBindVertexArray(0); 
    return true;
}


void EGLManager::DrawTexture(GLuint textureId) {
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(programId_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
    glUniform1i(textureUniformLocation_, 0);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void EGLManager::DrawLandmarks(const std::vector<float>& projectedCoordinates) {
    if (projectedCoordinates.empty()) return;
    glUseProgram(pointProgramId_);
    glBindVertexArray(landmarkVao_);
    glBindBuffer(GL_ARRAY_BUFFER, landmarkVbo_);
    // Upload the coordinate data to the GPU buffer
    glBufferSubData(GL_ARRAY_BUFFER, 0, projectedCoordinates.size() * sizeof(float), projectedCoordinates.data());
    // Render as points
    glDrawArrays(GL_POINTS, 0, projectedCoordinates.size() / 2);
    glBindVertexArray(0);
}

void EGLManager::DrawStrokeEffect(const std::vector<float> &meshVertexData, GLuint textureId) {
    if (meshVertexData.empty()) return;

    glUseProgram(meshProgramId_);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
    glUniform1i(meshTextureUniformLocation_, 0);

    glBindVertexArray(landmarkVao_);

    glUniform2f(meshDirectionUniformLocation_, +0.03f, -0.15f);

    glBindBuffer(GL_ARRAY_BUFFER, landmarkVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, meshVertexData.size() * sizeof(float), meshVertexData.data());

    glDrawElements(
        GL_TRIANGLES, 
        static_cast<GLsizei>(NUM_FACE_INDICES), 
        GL_UNSIGNED_SHORT, 
        (void*)0
    );
    glBindVertexArray(0);
}