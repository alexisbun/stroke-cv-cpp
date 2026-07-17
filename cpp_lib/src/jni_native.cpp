#include "camera_engine.h"
#include "mediapipe_face_mesh.h"
#include "lib.h"
#include <android/hardware_buffer.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/spdlog.h>
#include <stdint.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

JavaVM *g_JavaVM = nullptr;

extern "C" jint JNI_OnLoad(JavaVM *vm, void *reserved) {
  g_JavaVM = vm;
  try {
    auto android_logger =
    spdlog::android_logger_mt("stroke_cv_logger", "StrokeCV_Native");
    spdlog::set_default_logger(android_logger);
    spdlog::set_pattern("%v");
    spdlog::set_level(spdlog::level::debug);

    spdlog::info("spdlog initialized successfully in JNI_OnLoad!");
  } catch (const spdlog::spdlog_ex &ex) {
    __android_log_print(ANDROID_LOG_ERROR, "StrokeCV_Native",
                        "Failed to initialize spdlog in JNI_OnLoad: %s",
                        ex.what());
  }
  return JNI_VERSION_1_6;
}
extern "C" long long nativeAttach(void *env, void *surface, int32_t width,
                                  int32_t height) {
  // 1. Cast the generic pointers back to JNI types
  JNIEnv *jniEnv = reinterpret_cast<JNIEnv *>(env);
  jobject jniSurface = reinterpret_cast<jobject>(surface);
  if (jniEnv == nullptr || jniSurface == nullptr) {
    return 0;
  }
  ANativeWindow *window = ANativeWindow_fromSurface(jniEnv, jniSurface);
  if (window == nullptr) {
    return 0;
  }
  int32_t format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
  int32_t status =
      ANativeWindow_setBuffersGeometry(window, width, height, format);
  if (status < 0) {
    ANativeWindow_release(window);
    return 0;
  }
  CameraEngine *engine = new CameraEngine(window, width, height, format);
  return reinterpret_cast<int64_t>(engine);
}
extern "C" void nativeDetach(long long engineHandle) {
  if (engineHandle == 0) {
    return;
  }
  CameraEngine *engine = reinterpret_cast<CameraEngine *>(engineHandle);
  delete engine;
}

FaceMesh faceMesh;
extern "C" void initFaceMeshFromAsset(void* env_ptr, jobject j_asset_manager, const char* asset_name) {
    JNIEnv* env = reinterpret_cast<JNIEnv*>(env_ptr);
    AAssetManager* mgr = AAssetManager_fromJava(env, j_asset_manager);
    AAsset* asset = AAssetManager_open(mgr, asset_name, AASSET_MODE_BUFFER);
    if (asset) {
        size_t size = AAsset_getLength(asset);
        char* buffer = new char[size];
        AAsset_read(asset, buffer, size);
        AAsset_close(asset);
        faceMesh.InitializeFaceLandmarkerFromBuffer(buffer, size);
    }
}