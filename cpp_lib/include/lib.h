#ifdef __cplusplus
extern "C" {
#endif

long long nativeAttach(void *env, void *surface, int width, int height);
void nativeDetach(long long engineHandle);
double getEngineFps(long long engineHandle);

#ifdef __cplusplus
}
#endif
