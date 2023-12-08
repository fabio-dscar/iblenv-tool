#ifndef __IBL_IMAGE_H__
#define __IBL_IMAGE_H__

#include <glad/glad.h>

#include <cstring>
#include <memory>
#include <cmath>
#include <cassert>

namespace ibl {

struct ImageFormat {
    int width = 0, height = 0, depth = 0;
    int numChannels = 0;
    int compSize = 0;
};

struct RawImage {
    std::string name;
    std::byte* data = nullptr;
    ImageFormat fmt;
};

std::size_t ImageSize(const ImageFormat& fmt, int levels = 1);

class Image {
public:
    Image() = default;
    Image(ImageFormat format, int levels = 1);
    Image(ImageFormat format, int levels, const RawImage& source);
    Image(ImageFormat format, int levels, void* srcData);
    Image(ImageFormat format, std::unique_ptr<std::byte[]> imgPtr, int levels = 1);

    void copy(const RawImage& src);
    void copy(int dstX, int dstY, int srcX, int srcY, int lenX, int lenY,
              const Image& src);

    std::byte* dataPtr(int lvl = 0) const;

    std::size_t size() const;
    std::size_t size(int level) const;

    std::size_t pixelOffset() const { return compSize * numChan; }

    std::unique_ptr<std::byte[]> data = nullptr;
    int width = 0, height = 0, depth = 0;
    int levels = 1;
    int numChan = 0;
    int compSize = 1;
};

} // namespace ibl

#endif // __IBL_IMAGE_H__