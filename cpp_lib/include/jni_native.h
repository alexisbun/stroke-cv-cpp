// Here, this approach uses static resolution (name mangling) for my JNI-C+ bridge. 
// In the future, I may transition to dynamic registration if I were to encorperate more JNI funcitons.
#pragma once
#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/hardware_buffer.h>

#ifdef __cplusplus
extern "C"
{
#endif

// See TexturePlugin.kt for external Kotlin declaration of nativeAttach. 
JNIEXPORT jlong JNICALL
Java_com_example_camera_1mvp_TexturePlugin_nativeAttach(
    JNIEnv* env,
    jobject thiz,
    jobject surface,
    jint width,
    jint height
);

// See TexturePlugin.kt for external Kotlin declaration of nativeDetach. 
JNIEXPORT void JNICALL
Java_com_example_camera_1mvp_TexturePlugin_nativeDetach(
    JNIEnv* env,
    jobject thiz,
    jlong handle
);

}