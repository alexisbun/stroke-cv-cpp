#include "jni_native.h"
#include "camera_engine.h"

JNIEXPORT jlong JNICALL
Java_com_example_camera_1mvp_TexturePlugin_nativeAttach(
    JNIEnv *env,
    jobject thiz,
    jobject surface,
    jint width,
    jint height)
{
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr)
    {
        return 0; // Indicates failure to connect to JNI.
    }

    int32_t format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;

    int32_t status = ANativeWindow_setBuffersGeometry(window, width, height, format); // Returns 0 on success and negative value on error.
    if (status < 0)
    {
        ANativeWindow_release(window);
        window = nullptr;
        return 0;
    }

    CameraEngine *engine = new CameraEngine(window, width, height, format);

    return reinterpret_cast<jlong>(engine); // force JNI to treat CameraEngine memory address as a 'jlong' (i.e. 64-bit integer)
}

JNIEXPORT void JNICALL
Java_com_example_camera_1mvp_TexturePlugin_nativeDetach(
    JNIEnv *env,
    jobject thiz,
    jlong handle)
{
    if (handle == 0)
    {
        return;
    } // guard against null handles.

    CameraEngine *engine = reinterpret_cast<CameraEngine *>(handle);
    delete engine;
}
