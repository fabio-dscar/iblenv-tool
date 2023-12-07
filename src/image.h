#ifndef __IBL_IMAGE_H__
#define __IBL_IMAGE_H__

#include <glad/glad.h>

#include <cstring>
#include <memory>
#include <cmath>
#include <cassert>

namespace ibl {

// enum class ImageFormat {
//     UNKNOWN = 0,

//     R8,
//     RG8,
//     RGB8,

//     R16F,
//     RG16F,
//     RGB16F,

//     R32F,
//     RG32F,
//     RGB32F
// };

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

std::size_t ImageSizeLevels(const ImageFormat& fmt, int levels);

class Image {
public:
    Image() = default;
    Image(ImageFormat format, int levels = 1)
        : width(format.width), height(format.height), depth(format.depth), levels(levels),
          numChannels(format.numChannels), compSize(format.compSize) {}

    Image(ImageFormat format, int levels, const RawImage& source)
        : Image(format, levels) {
        copy(source);
    }

    Image(ImageFormat format, int levels, void* srcData) : Image(format, levels) {
        data = std::make_unique<std::byte[]>(size());
        std::memcpy(data.get(), srcData, size());
    }

    Image(ImageFormat format, std::unique_ptr<std::byte[]> imgPtr, int levels = 1)
        : Image(format, levels) {
        data = std::move(imgPtr);
    }

    void copy(const RawImage& src);

    std::byte* dataPtr(int lvl = 0) const {
        std::size_t prevLevelsSz = ImageSizeLevels(
            {width, height, depth, numChannels, compSize}, std::max(lvl - 1, 0));
        return &data.get()[prevLevelsSz];
    }

    std::size_t size(int level) const {
        int w = width * std::pow(0.5, level);
        int h = height * std::pow(0.5, level);
        int d = depth * std::pow(0.5, level);
        return w * std::max(h, 1) * std::max(d, 1) * compSize;
    }

    std::size_t size() const {
        return ImageSizeLevels({width, height, depth, numChannels, compSize}, levels);
    }

    std::size_t pixelOffset() const { return compSize * numChannels; }

    std::unique_ptr<std::byte[]> data = nullptr;
    int width = 0, height = 0, depth = 0;
    int levels = 1;
    int numChannels = 0;
    int compSize = 1;
};

} // namespace ibl

#endif // __IBL_IMAGE_H__