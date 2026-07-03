#include "jni_native.h"
#include <memory>

struct CameraEngine
{
    ANativeWindow *window = nullptr;
    int32_t width{0};
    int32_t height{0};
    int32_t format{0};
    // Placeholder for additional params for Camera API or OpenGL contexts.
    // Placeholder for constructors and class member functions.

    // CameraEngine(ANativeWindow *window,
    //              int32_t width,
    //              int32_t height,
    //              int32_t format)
    // {
    // }
    ~CameraEngine()
    {
        if (window != nullptr)
        {
            ANativeWindow_release(window);
            window = nullptr;
        }
    }

    
};

JNIEXPORT jlong JNICALL
Java_com_example_camera_1mvp_TexturePlugin_nativeAttach(
    JNIEnv *env,
    jobject thiz,
    jobject surface,
    jint width,
    jint height)
{
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    // if (window == nullptr)
    // {
    //     return 0;
    // } // Indicates failure to connect to JNI.

    int32_t format = AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420;
    ANativeWindow_setBuffersGeometry(window, width, height, format);

    CameraEngine* engine = new CameraEngine();
    engine->window = window;
    engine->width = width;
    engine->height = height;
    engine->format = format;

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
    } // guard against null handles (0)

    CameraEngine *engine = reinterpret_cast<CameraEngine *>(handle);

    if (engine->window != nullptr)
    {
        ANativeWindow_release(engine->window);
        engine->window = nullptr;
    }

    delete engine;
}
