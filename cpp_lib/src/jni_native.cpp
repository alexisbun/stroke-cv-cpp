#include "jni_native.h"
#include "camera_engine.h"

jlong nativeAttach(
    JNIEnv *env,
    jobject thiz, 
    jobject surface,
    jint width,
    jint height)
{
    auto android_logger = spdlog::android_logger_mt("stroke_cv_logger", "StrokeCV_Native");
    spdlog::set_default_logger(android_logger);
    spdlog::set_pattern("%v");
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("spdlog initialized successfully via nativeAttach");
    spdlog::info("Native attach called");

    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr)
    {
        return 0; // Indicates failure to connect to JNI.
    }
    spdlog::info("Window obtained");

    int32_t format = AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;

    int32_t status = ANativeWindow_setBuffersGeometry(window, width, height, format); // Returns 0 on success and negative value on error.
    if (status < 0)
    {
        ANativeWindow_release(window);
        window = nullptr;
        return 0;
    }

    CameraEngine *engine = new CameraEngine(window, width, height, format);
    spdlog::info("CameraEngine created");

    return reinterpret_cast<jlong>(engine); // treat CameraEngine memory address as a 64-bit integer
}

// C++ Implementation of nativeDetach
void nativeDetach(
    JNIEnv * env,   
    jobject thiz,  
    jlong handle)
{
    spdlog::info("native detach invoked");
    if (handle == 0)
    {
        return;
    } // guard against null handles.

    CameraEngine *engine = reinterpret_cast<CameraEngine *>(handle);
    delete engine;
}

// Method mapping table registering Kotlin/Java method names and signatures
static const JNINativeMethod gMethods[] = {
    {
        "nativeAttach",
        "(Landroid/view/Surface;II)J", 
        (void*)nativeAttach
    },
    {
        "nativeDetach",
        "(J)V",                        
        (void*)nativeDetach
    }
};

// Implement JNI_OnLoad to dynamically register native methods
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
    (void)reserved; // Suppress unused parameter warning
    
    JNIEnv *env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
    {
        return JNI_ERR;
    }

    // Find the class where the 'external' methods are declared
    jclass clazz = env->FindClass("com/example/camera_mvp/TexturePlugin");
    if (clazz == nullptr)
    {
        return JNI_ERR;
    }

    // Bind methods dynamically
    jint rc = env->RegisterNatives(
        clazz,
        gMethods,
        sizeof(gMethods) / sizeof(gMethods[0])
    );

    if (rc < 0)
    {
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}
