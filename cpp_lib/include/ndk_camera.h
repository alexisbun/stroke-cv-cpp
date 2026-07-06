#pragma once
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraError.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadataTags.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCaptureRequest.h>

#include <map>
#include <string>
#include <vector>

enum class CaptureSessionState : int32_t
{
    READY = 0, // session is ready but not streaming camera frames
    ACTIVE,    // session is streaming camera frames
    CLOSED,    // session is closed or initilizing
};

class NDKCamera
{
public:
    NDKCamera();
    ~NDKCamera();
    void EnumerateCamera();                    // Enumerate through camera's and select which to use.
    void CreateSession(ANativeWindow *window); // Calls ACameraDevice_createCaptureSession() to begin using user's camera.
    void StartPreview(bool start);
    ACameraDevice_stateCallbacks *GetDeviceListener();
    ACameraCaptureSession_stateCallbacks *GetSessionListener();

private:
    ACameraManager *cameraManager_;
    ACameraDevice *cameraDevice_;
    ACameraCaptureSession *captureSession_;
    ACaptureSessionOutputContainer *outputContainer;

    ANativeWindow *ndkWindow_;
    ACaptureSessionOutput *captureOutput_;
    ACameraOutputTarget *outputTarget;
    ACaptureRequest *captureRequest;

    std::string frontCameraId_;
    std::string backCameraId_;

    uint32_t cameraFacing_;
    uint32_t cameraOrientation_;

    bool isStreaming_;

    CaptureSessionState cameraState_;
};