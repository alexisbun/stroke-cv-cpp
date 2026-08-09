#ifdef __cplusplus
extern "C" {
#endif

long long nativeAttach(void *env, void *surface, int width, int height);
void nativeDetach(long long engineHandle);
double getEngineFps(long long engineHandle);
void initFaceMeshFromAsset(void* env_ptr, void* j_asset_manager, const char* asset_name);
// add method to initialize GCN model from assets directory in flutter project

#ifdef __cplusplus
}
#endif
