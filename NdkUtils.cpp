#include "NdkUtils.hpp"

using namespace std;
using namespace PirateNdkEngine;

vector<char> PirateNdkEngine::readFile(AAssetManager *mgr, const string &file_path)
{
    AAsset *file = AAssetManager_open(mgr, file_path.c_str(), AASSET_MODE_BUFFER);
    if (!file)
    {
        throw runtime_error{"Unknown error. Couldn't open file."};
    }

    vector<char> file_contents(AAsset_getLength(file));

    if (AAsset_read(file, file_contents.data(), file_contents.size()) != file_contents.size())
    {
        AAsset_close(file);
        throw runtime_error{"Unknown error. Couldn't load file contents."};
    }
    AAsset_close(file);

    return file_contents;
}

// This value is 2 ^ 18 - 1, and is used to clamp the RGB values before their
// ranges
// are normalized to eight bits.
static const int kMaxChannelValue = 262143;

uint32_t PirateNdkEngine::YUV2RGBA(int nY, int nU, int nV)
{
    nY -= 16;
    nU -= 128;
    nV -= 128;
    if (nY < 0) nY = 0;

    // This is the floating point equivalent. We do the conversion in integer
    // because some Android devices do not have floating point in hardware.
    // nR = (int)(1.164 * nY + 1.596 * nV);
    // nG = (int)(1.164 * nY - 0.813 * nV - 0.391 * nU);
    // nB = (int)(1.164 * nY + 2.018 * nU);

    int nR = (int) (1192 * nY + 1634 * nV);
    int nG = (int) (1192 * nY - 833 * nV - 400 * nU);
    int nB = (int) (1192 * nY + 2066 * nU);

    nR = std::min(kMaxChannelValue, std::max(0, nR));
    nG = std::min(kMaxChannelValue, std::max(0, nG));
    nB = std::min(kMaxChannelValue, std::max(0, nB));

    nR = (nR >> 10) & 0xff;
    nG = (nG >> 10) & 0xff;
    nB = (nB >> 10) & 0xff;

    return 0xff000000 | (nR << 16) | (nG << 8) | nB;
}

/*
 *   Converting yuv to RGB
 *   Refer to:
 * https://mathbits.com/MathBits/TISection/Geometry/Transformations2.htm
 */
void PirateNdkEngine::YUV420PTORGBA(uint32_t *out, AImage *image)
{
#ifndef NDEBUG
    AImageCropRect srcRect;
    AImage_getCropRect(image, &srcRect);

    int32_t srcFormat = -1;
    AImage_getFormat(image, &srcFormat);
    assert(AIMAGE_FORMAT_YUV_420_888 == srcFormat);
    int32_t srcPlanes = 0;
    AImage_getNumberOfPlanes(image, &srcPlanes);
    assert(srcPlanes == 3);
#endif


    int32_t yStride, uvStride;
    uint8_t *yPixel, *uPixel, *vPixel;
    int32_t yLen, uLen, vLen;
    AImage_getPlaneRowStride(image, 0, &yStride);
    AImage_getPlaneRowStride(image, 1, &uvStride);
    auto res = AImage_getPlaneData(image, 0, &yPixel, &yLen);
    res = AImage_getPlaneData(image, 1, &vPixel, &vLen);
    res = AImage_getPlaneData(image, 2, &uPixel, &uLen);

    int32_t width, height;
    AImage_getWidth(image, &width);
    AImage_getHeight(image, &height);

    for (int32_t y = 0; y < height; y++)
    {
        const uint8_t *pY = yPixel + yStride * y;

        int32_t uv_row_start = uvStride * (y >> 1);
        const uint8_t *pU = uPixel + uv_row_start;
        const uint8_t *pV = vPixel + uv_row_start;

        for (int32_t x = 0; x < width; x++)
        {
            const int32_t uv_offset = (x >> 1);
            out[width * y + x] = YUV2RGBA(pY[x], pU[uv_offset], pV[uv_offset]);
        }
    }
}

AndroidBuffer::AndroidBuffer(android_LogPriority priority) : priority_{priority}
{
    setp(buffer_, buffer_ + kBufferSize - 1);
}

int32_t AndroidBuffer::overflow(int32_t c)
{
    if (c == traits_type::eof())
    {
        *this->pptr() = traits_type::to_char_type(c);
        this->sbumpc();
    }
    return this->sync() ? traits_type::eof() : traits_type::not_eof(c);
}

int32_t AndroidBuffer::sync()
{
    int32_t rc = 0;
    if (this->pbase() != this->pptr())
    {
        char writebuf[kBufferSize + 1];
        memcpy(writebuf, this->pbase(), this->pptr() - this->pbase());
        writebuf[this->pptr() - this->pbase()] = '\0';

        rc = __android_log_write(priority_, "std", writebuf) > 0;
        this->setp(buffer_, buffer_ + kBufferSize - 1);
    }
    return rc;
}
