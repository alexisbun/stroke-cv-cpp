#pragma once
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCaptureRequest.h>
#include <android/native_window.h>

#include <media/NdkImageReader.h>
#include <media/NdkImage.h>

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
    void CreateSession(ANativeWindow *window); // Will pass the AImageReader's window here
    void StartPreview(bool start);

    ACameraDevice_stateCallbacks *GetDeviceListener();
    ACameraCaptureSession_stateCallbacks *GetSessionListener();

private:
    ACameraManager *cameraManager_;
    ACameraDevice *cameraDevice_;
    ACameraCaptureSession *captureSession_;
    ACaptureSessionOutputContainer *outputContainer_;

    ANativeWindow *readerWindow_;
    ACaptureSessionOutput *captureOutput_;
    ACameraOutputTarget *outputTarget_;
    ACaptureRequest *captureRequest_;

    std::string frontCameraId_;
    std::string backCameraId_;

    uint32_t cameraFacing_;
    uint32_t cameraOrientation_;

    bool isStreaming_;
    CaptureSessionState cameraState_;
};