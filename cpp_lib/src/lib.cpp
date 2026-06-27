#include "lib.h"

#include <camera/NdkCameraManager.h>        // manages system level camera resaurces
#include <camera/NdkCameraDevice.h>         // manages open connection to camera
#include <camera/NdkCameraCaptureSession.h> // manages active camera streaming
#include <camera/NdkCaptureRequest.h>       // ACameraCaptureRequest: configures the settings for specific camera frames and maps graphics buffers to camera requests
#include <camera/NdkCameraMetadata.h>       // contains containers, tags for camera metadata.

#include <android/log.h>

#include <jni.h>
#include <android/native_window.h>
#include <android/surface_texture.h>
#include <android/native_window_jni.h>

#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h> // For GL_TEXTURE_EXTERNAL_OES

void print(const char *message)
{
    __android_log_print(ANDROID_LOG_INFO, "C++", "%s", message);
}

GLuint initOpenGLExternalTexture()
{
    // print("Calling initOpenGLExternalTexture");

    // use the GL_TEXTURE_EXTERNAL_OES target
    GLuint textureid = 0; // implementation note: typedef unsigned int GLuint;-> GLuint is a unsigned integer value

    glGenTextures(1, &textureid);                      // generate name/ID for texture object
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureid); // bind external texture to current texture object

    // configure texture properties / sampling parameters (mandatory fields)
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return textureid;
}

// __attribute__((visibility("default"))) __attribute__((used))
//     void init_native_logging() {
//         auto android_logger = spdlog::android_logger_mt("android_logger", "MyAppTag", true);
//         spdlog::set_default_logger(android_logger);
//         spdlog::info("Spdlog initialized successfully);
//     }
int sum(int a, int b)
{
    return a + b;
}
int multiply(int a, int b)
{
    return a * b;
}

int main()
{
    // spdlog::info("android_logger", "MyAppTag");
    // spdlog::info("Hello kitty");
    // std::cout << sum(3, 2) << '\n';
    // std::cout << multiply(3, 4) << '\n';
}