#pragma once

#include <memory>
#include <string>
#include <iostream>
#include <cassert>

#include <camera/NdkCameraManager.h>
#include <android/native_window.h>

#include "Logging.hpp"

template <typename T>
class RangeValue
{
public:
    T min_, max_;
    /**
     * return absolute value from relative value
     * value: in percent (50 for 50%)
     */
    T value(int percent) {
      return static_cast<T>(min_ + (max_ - min_) * percent / 100);
    }
    RangeValue() { min_ = max_ = static_cast<T>(0); }

    bool Supported(void) const { return (min_ != max_); }
};

/**
 * ndk camera class
 */
class CameraNDK
{
private:
    std::unique_ptr<ACameraManager, decltype(&ACameraManager_delete)> mgr_;
    std::unique_ptr<ACameraDevice, decltype(&ACameraDevice_close)> device_;
    std::unique_ptr<ACaptureRequest, decltype(&ACaptureRequest_free)> request_;
    std::unique_ptr<ACaptureSessionOutputContainer, decltype(&ACaptureSessionOutputContainer_free)>
    output_container_;
    std::unique_ptr<ACaptureSessionOutput, decltype(&ACaptureSessionOutput_free)> output_;
    std::unique_ptr<ACameraOutputTarget, decltype(&ACameraOutputTarget_free)> target_;
    std::unique_ptr<ACameraCaptureSession, decltype(&ACameraCaptureSession_close)> capture_session_;

    // set up exposure control
    int64_t exposureTime_;
    RangeValue<int64_t> exposureRange_;
    int32_t sensitivity_;
    RangeValue<int32_t> sensitivityRange_;
    volatile bool valid_;

    // callbacks
    // camera disconnected, close it
    static void on_device_disconnected(void* a_obj, ACameraDevice* a_device)
    {
        LOGW("device is disconnected");
        reinterpret_cast<CameraNDK*>(a_obj)->device_.reset(nullptr);
    }

    static void on_device_error(void* a_obj, ACameraDevice* a_device, int a_err_code)
    {
        LOGE("device error {}", a_err_code);
    }

    static void on_session_closed(void* a_obj, ACameraCaptureSession* a_session)
    {}

    static void on_session_ready(void* a_obj, ACameraCaptureSession* a_session)
    {}

    static void on_session_active(void* a_obj, ACameraCaptureSession* a_session)
    {}

    ACameraDevice_stateCallbacks dev_state_cbs_
            {
                    this,
                    on_device_disconnected,
                    on_device_error
            };

    ACameraCaptureSession_stateCallbacks cap_state_cbs_
            {
                    this,
                    on_session_closed,
                    on_session_ready,
                    on_session_active
            };

    /**
     * get camera id
     * @param mgr camera manager
     * @return camera id
     */
    std::string GetCameraId(ACameraManager *mgr);

public:
    explicit CameraNDK();
    ~CameraNDK();

    void create(ANativeWindow* window);
    /**
     * start capture
     */
    void start_capturing();

    /**
     * stop capture
     */
    void stop_capturing();
};