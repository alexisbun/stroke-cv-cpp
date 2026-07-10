#include "ndk_camera.h"

static void onDeviceDisconnected(void* context, ACameraDevice* device)
{
    (void)device;
    (void)context;
}
static void onDeviceError(void* context, ACameraDevice* device, int error)
{
    (void)device;
    (void)error;
    (void)context;
}
static void onSessionClosed(void* context, ACameraCaptureSession* session)
{
    (void)session;
    (void)context;
}
static void onSessionReady(void* context, ACameraCaptureSession* session)
{
    (void)session;
    (void)context;
}
static void onSessionActive(void* context, ACameraCaptureSession* session)
{
    (void)session;
    (void)context;
}

NDKCamera::NDKCamera()
    : cameraManager_(nullptr),
      cameraDevice_(nullptr),
      captureSession_(nullptr),
      outputContainer_(nullptr),
      readerWindow_(nullptr),
      captureOutput_(nullptr),
      outputTarget_(nullptr),
      captureRequest_(nullptr),
      cameraFacing_(ACAMERA_LENS_FACING_FRONT),
      cameraOrientation_(0),
      isStreaming_(false),
      cameraState_(CaptureSessionState::CLOSED)
{
}

NDKCamera::~NDKCamera() 
{
    StartPreview(false);

    if (captureSession_ != nullptr) {
        ACameraCaptureSession_close(captureSession_);
        captureSession_ = nullptr;
    }
    if (captureRequest_ != nullptr) {
        ACaptureRequest_removeTarget(captureRequest_, outputTarget_);
        ACaptureRequest_free(captureRequest_);
        captureRequest_ = nullptr;
    }
    if (outputTarget_ != nullptr) {
        ACameraOutputTarget_free(outputTarget_);
        outputTarget_ = nullptr;
    }
    if (outputContainer_ != nullptr) {
        if (captureOutput_ != nullptr) {
            ACaptureSessionOutputContainer_remove(outputContainer_, captureOutput_);
            ACaptureSessionOutput_free(captureOutput_);
            captureOutput_ = nullptr;
        }
        ACaptureSessionOutputContainer_free(outputContainer_);
        outputContainer_ = nullptr;
    }
    if (cameraDevice_ != nullptr) {
        ACameraDevice_close(cameraDevice_);
        cameraDevice_ = nullptr;
    }
    if (cameraManager_ != nullptr) {
        ACameraManager_delete(cameraManager_);
        cameraManager_ = nullptr;
    }
    if (readerWindow_ != nullptr) {
        ANativeWindow_release(readerWindow_);
        readerWindow_ = nullptr;
    }
}

void NDKCamera::EnumerateCamera() 
{
    cameraManager_ = ACameraManager_create();
    if (cameraManager_ == nullptr) {
        return;
    }

    ACameraIdList* idList = nullptr;
    camera_status_t status = ACameraManager_getCameraIdList(cameraManager_, &idList); // query list of all physical camera's on the device.
    if (status != ACAMERA_OK || idList == nullptr) {
        return; // early return if no camera's are detected on the device.
    }
    for (int i = 0; i < idList->numCameras; ++i) {
        const char* id = idList->cameraIds[i];
        ACameraMetadata* chars = nullptr;
        
        status = ACameraManager_getCameraCharacteristics(cameraManager_, id, &chars); // get camera features (such as lens orientation)
        if (status != ACAMERA_OK || chars == nullptr) {
            continue;
        }

        // determine if camera lens orientation is front or back.
        ACameraMetadata_const_entry entry; // handle to hold camera feature tags
        status = ACameraMetadata_getConstEntry(chars, ACAMERA_LENS_FACING, &entry); // retrieve lens direction tag and assign to entry.
        if (status == ACAMERA_OK) {
            auto facing = static_cast<acamera_metadata_enum_android_lens_facing_t>(entry.data.u8[0]);
            
            if (facing == ACAMERA_LENS_FACING_FRONT) {
                frontCameraId_ = id;
            } else if (facing == ACAMERA_LENS_FACING_BACK) {
                backCameraId_ = id;
            }
        }
        ACameraMetadata_free(chars); // Call to free object from memory.
    }
    ACameraManager_deleteCameraIdList(idList); 
    std::string activeId = frontCameraId_.empty() ? backCameraId_ : frontCameraId_;
    if (activeId.empty()) {
        return;
    }

    ACameraMetadata* chars = nullptr;
    camera_status_t charsStatus = ACameraManager_getCameraCharacteristics(
        cameraManager_,
        activeId.c_str(), &chars
    );

    bool is60FpsSupported = false;
    if (charsStatus == ACAMERA_OK && chars != nullptr) {
        ACameraMetadata_const_entry fpsEntry;
        camera_status_t status = ACameraMetadata_getConstEntry(
            chars,
            ACAMERA_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES,
            &fpsEntry
        );

        if (status == ACAMERA_OK) {
        for (uint32_t i = 0; i < fpsEntry.count; i += 2) {
            int32_t minFps = fpsEntry.data.i32[i];
            int32_t maxFps = fpsEntry.data.i32[i + 1];

            if (minFps >= 60 || maxFps >= 60) {
                    is60FpsSupported = true;
                    break;
            }
        }
    }
    ACameraMetadata_free(chars);
    }

    ACameraManager_openCamera(
        cameraManager_,
        activeId.c_str(),
        GetDeviceListener(),
        &cameraDevice_
    );
}

void NDKCamera::CreateSession(ANativeWindow* readerWindow) 
{
    if (cameraDevice_ == nullptr || readerWindow == nullptr) {
        return;
    }
    readerWindow_ = readerWindow;
    ANativeWindow_acquire(readerWindow_);
    ACaptureSessionOutputContainer_create(&outputContainer_);
    ACaptureSessionOutput_create(readerWindow_, &captureOutput_);
    ACaptureSessionOutputContainer_add(outputContainer_, captureOutput_);
    ACameraOutputTarget_create(readerWindow_, &outputTarget_);
    ACameraDevice_createCaptureSession(
        cameraDevice_,
        outputContainer_,
        GetSessionListener(),
        &captureSession_
    );
    ACameraDevice_createCaptureRequest(cameraDevice_, TEMPLATE_PREVIEW, &captureRequest_);

    int32_t targetFps[2] = {60, 60};

    camera_status_t fpsStatus = ACaptureRequest_setEntry_i32(
        captureRequest_, 
        ACAMERA_CONTROL_AE_TARGET_FPS_RANGE, 
        2, 
        targetFps
    );

    ACaptureRequest_addTarget(captureRequest_, outputTarget_);
}

void NDKCamera::StartPreview(bool start) 
{
    if (captureSession_ == nullptr || captureRequest_ == nullptr) { 
        return; 
    } 
    if (start) {
        ACameraCaptureSession_setRepeatingRequest(
            captureSession_,
            nullptr,
            1,
            &captureRequest_,
            nullptr
        );
        isStreaming_ = true;
        cameraState_ = CaptureSessionState::ACTIVE;
    } else {
        ACameraCaptureSession_stopRepeating(captureSession_);
        isStreaming_ = false;
        cameraState_ = CaptureSessionState::READY;
    }
}

ACameraDevice_stateCallbacks* NDKCamera::GetDeviceListener() 
{
    static ACameraDevice_stateCallbacks callbacks = {
        .context = this,
        .onDisconnected = onDeviceDisconnected,
        .onError = onDeviceError
    };
    return &callbacks;
}

ACameraCaptureSession_stateCallbacks* NDKCamera::GetSessionListener() 
{
    static ACameraCaptureSession_stateCallbacks callbacks = {
        .context = this,
        .onClosed = onSessionClosed,
        .onReady = onSessionReady,
        .onActive = onSessionActive
    };
    return &callbacks;
}

