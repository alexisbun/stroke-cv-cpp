#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

long long nativeAttach(void *env, void *surface, int width, int height);
void nativeDetach(long long engineHandle);
double getEngineFps(long long engineHandle);
void initFaceMeshFromAsset(void* env_ptr, jobject j_asset_manager, const char* asset_name);

#ifdef __cplusplus
}
#endif
