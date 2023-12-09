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
    int levels = 1;
};

struct CopyExtents {
    int toX, toY;
    int fromX, fromY;
    int sizeX, sizeY;
};

// Pointer to image data from image loading libraries
struct RawImage {
    std::string name;
    std::byte* data = nullptr;
    ImageFormat fmt;
};

std::size_t ImageSize(const ImageFormat& fmt);

// Mipmapped image
class Image {
public:
    Image() = default;
    Image(const ImageFormat& format);
    Image(const ImageFormat& format, const Image& src, CopyExtents extents, int lvl = 0);
    Image(const ImageFormat& format, const RawImage& source);
    Image(const ImageFormat& format, std::unique_ptr<std::byte[]> imgPtr);

    void copy(const RawImage& src);
    void copy(int dstX, int dstY, int srcX, int srcY, int lenX, int lenY, int toLvl,
              const Image& src);
    void copy(CopyExtents extents, const Image& src, int toLvl = 0);
    void copy(const Image& src, int toLvl = 0);

    std::byte* data(int lvl = 0) const;
    std::byte* pixel(int x, int y, int lvl = 0) const;

    std::size_t size() const;
    std::size_t size(int lvl) const;

    std::size_t pixelSize() const { return compSize * numChan; }

    ImageFormat format() const;
    ImageFormat format(int level) const;

    std::unique_ptr<std::byte[]> imgData = nullptr;
    int width = 0, height = 0, depth = 0;
    int levels = 1;
    int numChan = 0;
    int compSize = 1;
};

struct SpanExtents {
    int fromX = 0, fromY = 0;
    int sizeX = 0, sizeY = 0;
    int lvl = 0;
};

// Non owning reference to an image (including all levels),
// one image level or a portion of an image level
class ImageSpan {
public:
    ImageSpan(const Image& image);
    ImageSpan(const Image& image, int lvl);
    ImageSpan(const Image& image, SpanExtents extents);

    ImageFormat format() const {
        return {width, height, depth, numChan, compSize, levels};
    }

    const std::byte* data() const { return start; }
    std::size_t size() const { return spanSize; }

    const std::byte* start;
    std::size_t spanSize;

    int width = 0, height = 0, depth = 0;
    int levels = 1;
    int numChan = 0;
    int compSize = 1;
};

} // namespace ibl

#endif // __IBL_IMAGE_H__