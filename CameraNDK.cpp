//
// Created by ylm on 3/19/2021.
//

#include "CameraNDK.h"

using namespace std;

/**
 * Range of Camera Exposure Time:
 *     Camera's capability range have a very long range which may be disturbing
 *     on camera. For this sample purpose, clamp to a range showing visible
 *     video on preview: 100000ns ~ 250000000ns
 */
static const uint64_t kMinExposureTime = static_cast<uint64_t>(1000000);
static const uint64_t kMaxExposureTime = static_cast<uint64_t>(250000000);

CameraNDK::CameraNDK() :
    mgr_(nullptr, ACameraManager_delete),
    device_(nullptr, ACameraDevice_close),
    request_(nullptr, ACaptureRequest_free),
    output_container_(nullptr, ACaptureSessionOutputContainer_free),
    output_(nullptr, ACaptureSessionOutput_free),
    target_(nullptr, ACameraOutputTarget_free),
    capture_session_(nullptr, ACameraCaptureSession_close)
{
    // create camera manager
    auto pt = ACameraManager_create();
    mgr_.reset(pt);
    if (mgr_ == nullptr)
        throw runtime_error("failed to create camera manager");
}

void CameraNDK::create(ANativeWindow *window)
{
    // open camera
    {
        // pick up a front/back facing camera to preview
        string id = GetCameraId(mgr_.get());

        // open
        auto pt = device_.release();
        auto res = ACameraManager_openCamera(mgr_.get(), id.c_str(), &dev_state_cbs_, &pt);
        if(res != ACAMERA_OK)
            throw runtime_error("can't open camera " + to_string(res));
        device_.reset(pt);

        // Initialize camera controls(exposure time and sensitivity), pick
        // up value of 2% * range + min as starting value (just a number, no magic)
        ACameraMetadata* metadataObj;
        ACameraManager_getCameraCharacteristics(mgr_.get(), id.c_str(), &metadataObj);
        ACameraMetadata_const_entry val = {0,};
        camera_status_t status = ACameraMetadata_getConstEntry(
                metadataObj, ACAMERA_SENSOR_INFO_EXPOSURE_TIME_RANGE, &val);
        if (status == ACAMERA_OK) {
            exposureRange_.min_ = val.data.i64[0];
            if (exposureRange_.min_ < kMinExposureTime) {
                exposureRange_.min_ = kMinExposureTime;
            }
            exposureRange_.max_ = val.data.i64[1];
            if (exposureRange_.max_ > kMaxExposureTime) {
                exposureRange_.max_ = kMaxExposureTime;
            }
            exposureTime_ = exposureRange_.value(2);
        } else {
            LOGW("Unsupported ACAMERA_SENSOR_INFO_EXPOSURE_TIME_RANGE");
            exposureRange_.min_ = exposureRange_.max_ = 0l;
            exposureTime_ = 0l;
        }
        status = ACameraMetadata_getConstEntry(
                metadataObj, ACAMERA_SENSOR_INFO_SENSITIVITY_RANGE, &val);

        if (status == ACAMERA_OK){
            sensitivityRange_.min_ = val.data.i32[0];
            sensitivityRange_.max_ = val.data.i32[1];

            sensitivity_ = sensitivityRange_.value(2);
        } else {
            LOGW("failed for ACAMERA_SENSOR_INFO_SENSITIVITY_RANGE");
            sensitivityRange_.min_ = sensitivityRange_.max_ = 0;
            sensitivity_ = 0;
        }
    }

    // create capture session output container
    {
        auto pt = output_container_.release();
        auto res = ACaptureSessionOutputContainer_create(&pt);
        if(res != ACAMERA_OK)
            throw runtime_error("can't create capture session output container " + to_string(res));
        output_container_.reset(pt);
    }

    // create capture session output
    {
        auto pt = output_.release();
        auto res = ACaptureSessionOutput_create(window, &pt);
        if(res != ACAMERA_OK)
            throw runtime_error("can't create capture session output " + to_string(res));
        output_.reset(pt);
    }

    // add output to container
    {
        auto res = ACaptureSessionOutputContainer_add(output_container_.get(), output_.get());
        if(res != ACAMERA_OK)
            throw runtime_error("can't add output to container " + to_string(res));
    }

    // create capture session
    {
        auto pt = capture_session_.release();
        auto res = ACameraDevice_createCaptureSession(device_.get(), output_container_.get(),
                &cap_state_cbs_, &pt);
        if(res != ACAMERA_OK)
            throw runtime_error("can't create capture session " + to_string(res));
        capture_session_.reset(pt);
    }

    // create request
    {
        auto pt = request_.release();
        auto res = ACameraDevice_createCaptureRequest(device_.get(), TEMPLATE_PREVIEW, &pt);
        if(res != ACAMERA_OK)
            throw runtime_error("can't create capture request " + to_string(res));
        request_.reset(pt);

        /*
        uint8_t aeModeOff = ACAMERA_CONTROL_AE_MODE_OFF;
        ACaptureRequest_setEntry_u8(request_.get(), ACAMERA_CONTROL_AE_MODE, 1, &aeModeOff);
        ACaptureRequest_setEntry_i32(request_.get(), ACAMERA_SENSOR_SENSITIVITY, 1, &sensitivity_);
        ACaptureRequest_setEntry_i64(request_.get(), ACAMERA_SENSOR_EXPOSURE_TIME, 1, &exposureTime_);
        */
    }

    // create target
    {
        auto pt = target_.release();
        auto res = ACameraOutputTarget_create(window, &pt);
        if(res != ACAMERA_OK)
            throw runtime_error("can't create camera output target " + to_string(res));
        target_.reset(pt);
    }

    // add request to target
    {
        auto res = ACaptureRequest_addTarget(request_.get(), target_.get());
        if(res != ACAMERA_OK)
            throw runtime_error("can't add request to target" + to_string(res));
    }

    clog << "camera created" << endl;
}

CameraNDK::~CameraNDK()
{
    clog << "camera clear" << endl;
}

std::string CameraNDK::GetCameraId(ACameraManager *mgr)
{
    ACameraIdList *cameraIds = nullptr;
    auto result = ACameraManager_getCameraIdList(mgr, &cameraIds);
    if(result != ACAMERA_OK)
    {
        throw runtime_error("cannot get camera id list");
    }

    std::string id;

    for (int i = 0; i < cameraIds->numCameras; ++i)
    {
        const char *id_temp = cameraIds->cameraIds[i];

        ACameraMetadata *metadata = nullptr;
        ACameraManager_getCameraCharacteristics(mgr, id_temp, &metadata);

        ACameraMetadata_const_entry lensInfo = {0};
        ACameraMetadata_getConstEntry(metadata, ACAMERA_LENS_FACING, &lensInfo);

        auto facing = static_cast<acamera_metadata_enum_android_lens_facing_t>(
                lensInfo.data.u8[0]);

        // found a front/back facing camera
        if (facing == ACAMERA_LENS_FACING_BACK)
        {
            id = id_temp;
            break;
        }

        ACameraMetadata_free(metadata);
    }

    ACameraManager_deleteCameraIdList(cameraIds);

    return id;
}

void CameraNDK::start_capturing()
{
    auto pt = request_.release();
    ACameraCaptureSession_setRepeatingRequest(capture_session_.get(), nullptr, 1, &pt,
                                              nullptr);
    request_.reset(pt);
}

void CameraNDK::stop_capturing()
{
    ACameraCaptureSession_stopRepeating(capture_session_.get());
}