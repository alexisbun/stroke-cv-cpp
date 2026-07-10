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

#include "lib.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/android_sink.h>
#include "camera_engine.h"

int sum(int a, int b)
{
    return a + b;
}
int multiply(int a, int b)
{
    return a * b;
}

extern "C" double getEngineFps(long long engineHandle) {
    if (engineHandle == 0) {
        return 0.0;
    }
    CameraEngine *engine = reinterpret_cast<CameraEngine *>(engineHandle);
    return engine->GetFps(); 
}

