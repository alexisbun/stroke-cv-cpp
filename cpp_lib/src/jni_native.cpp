#include "camera_engine.h"
#include "mediapipe_face_mesh.h"
#include "lib.h"
#include "mobile_unet.h"
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
  // Cast C pointers back to JNI types to extract surface.
  JNIEnv *jniEnv = reinterpret_cast<JNIEnv *>(env);
  jobject jniSurface = reinterpret_cast<jobject>(surface);
  if (jniEnv == nullptr || jniSurface == nullptr) {
    spdlog::info("JNI handles are nullptr!");
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
extern "C" void initFaceMeshFromAsset(void* env_ptr, void* j_asset_manager, const char* asset_name) {
    JNIEnv* env = reinterpret_cast<JNIEnv*>(env_ptr);
    jobject asset_manager_obj = reinterpret_cast<jobject>(j_asset_manager);
    if (env == nullptr || asset_manager_obj == nullptr) {
      spdlog::error("initFaceMeshFromAsset: JNI handles are nullptr!");
      return;
    }
    AAssetManager* mgr = AAssetManager_fromJava(env, asset_manager_obj);
    if (!mgr) {
      spdlog::error("initFaceMeshFromAsset: Failed to get AAssetManager from Java!");
      return;
    }
    spdlog::info("initFaceMeshFromAsset: Attempting to open asset: {}", asset_name);
    AAsset* asset = AAssetManager_open(mgr, asset_name, AASSET_MODE_BUFFER);
    if (asset) {
      size_t size = AAsset_getLength(asset);
      spdlog::info("initFaceMeshFromAsset: Successfully opened asset. Size: {} bytes", size);
      std::vector<char> buffer(size); // use std::vector to manage memory automatically when copying buffer, prevent memory leaks.
      AAsset_read(asset, buffer.data(), size);
      AAsset_close(asset);
      faceMesh.InitializeFaceLandmarkerFromBuffer(buffer.data(), size);
    } else {
      spdlog::error("initFaceMeshFromAsset: Failed to open asset '{}' from AAssetManager!", asset_name);
    }
}

StrokeModelInference strokeModelInference;
extern "C" void initGCNModelFromAsset(void* env_ptr, void* j_asset_manager, const char* asset_name) {
    JNIEnv* env = reinterpret_cast<JNIEnv*>(env_ptr);
    jobject asset_manager_obj = reinterpret_cast<jobject>(j_asset_manager);
    if (env == nullptr || asset_manager_obj == nullptr) {
      spdlog::error("initGCNModelFromAsset: JNI handles are nullptr!");
      return;
    }
    AAssetManager* mgr = AAssetManager_fromJava(env, asset_manager_obj);
    if (!mgr) {
      spdlog::error("initGCNModelFromAsset: Failed to get AAssetManager from Java!");
      return;
    }
    spdlog::info("initGCNModelFromAsset: Attempting to open asset: {}", asset_name);
    AAsset* asset = AAssetManager_open(mgr, asset_name, AASSET_MODE_BUFFER);
    if (asset) {
      size_t size = AAsset_getLength(asset);
      spdlog::info("initGCNModelFromAsset: Successfully opened asset. Size: {} bytes", size);
      std::vector<char> buffer(size); // use std::vector to manage memory automatically, prevent memory leaks.
      AAsset_read(asset, buffer.data(), size);
      AAsset_close(asset);
      strokeModelInference.InitializeModelFromBuffer(buffer.data(), size);
    } else {
        spdlog::error("initGCNModelFromAsset: Failed to open asset '{}' from AAssetManager!", asset_name);
    }
}

MobileUNet mobileUNetInference;
extern "C" void initMobileUNetFromAsset(void* env_ptr, void* j_asset_manager, const char* asset_name) {
  if (env_ptr == nullptr || j_asset_manager == nullptr || asset_name == nullptr) {
    spdlog::error("initMobileUNetFromAsset: JNI handles are nullptr!");
    return;
  }
  JNIEnv* env = reinterpret_cast<JNIEnv*>(env_ptr);
  jobject asset_manager_obj = reinterpret_cast<jobject>(j_asset_manager);
  AAssetManager* mgr = AAssetManager_fromJava(env, asset_manager_obj);
  if (!mgr) {
    spdlog::error("initMobileUNetFromAsset: Failed to get AAssetManager from Java!");
    return;
  }
  spdlog::info("initMobileUNetFromAsset: Attempting to open asset: {}", asset_name);
  AAsset* asset = AAssetManager_open(mgr, asset_name, AASSET_MODE_BUFFER);
  if (asset) {
    size_t size = AAsset_getLength(asset);
    spdlog::info("initMobileUNetFromAsset: Successfully opened asset. Size: {} bytes", size);
    std::vector<char> buffer(size); // use std::vector to manage memory automatically, prevent memory leaks.
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);
    mobileUNetInference.InitializeModelFromBuffer(buffer.data(), size);
  } else {
     spdlog::error("initMobileUNetFromAsset: Failed to open asset '{}' from AAssetManager!", asset_name);
  }
}
