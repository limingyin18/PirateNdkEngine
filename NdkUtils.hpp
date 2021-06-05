#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <cassert>

#include <android/asset_manager.h>
#include <android/log.h>
#include <android/imagedecoder.h>
#include <android/native_window.h>
#include <media/NdkImage.h>

namespace PirateNdkEngine
{
std::vector<char> readFile(AAssetManager *mgr, const std::string &file_path);

// yuv420p -> rgba
void YUV420PTORGBA(uint32_t *out, AImage *image);
uint32_t YUV2RGBA(int nY, int nU, int nV);
// Helpder class to forward the cout/cerr output to logcat derived from:
// http://stackoverflow.com/questions/8870174/is-stdcout-usable-in-android-ndk
class AndroidBuffer : public std::streambuf
{
public:
    explicit AndroidBuffer(android_LogPriority priority);

private:
    static const int32_t kBufferSize = 128;

    int32_t overflow(int32_t c) override;

    int32_t sync() override;

    android_LogPriority priority_ = ANDROID_LOG_INFO;
    char buffer_[kBufferSize]{};
};




}