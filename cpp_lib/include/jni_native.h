#pragma once
#include <android/hardware_buffer.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <spdlog/sinks/android_sink.h>
#include <spdlog/spdlog.h>

#ifdef __cplusplus
extern "C" {
#endif

jlong nativeAttach(
    JNIEnv *env, jobject thiz, jobject surface, jint width, jint height);

void nativeDetach(
    JNIEnv *env, jobject thiz, jlong handle);

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);

#ifdef __cplusplus
}
#endif
