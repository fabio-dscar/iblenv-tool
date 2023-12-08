#include <image.h>

using namespace ibl;

Image::Image(ImageFormat format, int levels)
    : width(format.width), height(format.height), depth(format.depth), levels(levels),
      numChan(format.numChannels), compSize(format.compSize) {
    data = std::make_unique<std::byte[]>(size());
}

Image::Image(ImageFormat format, int levels, const RawImage& source)
    : Image(format, levels) {
    copy(source);
}

Image::Image(ImageFormat format, int levels, void* srcData) : Image(format, levels) {
    data = std::make_unique<std::byte[]>(size());
    std::memcpy(data.get(), srcData, size());
}

Image::Image(ImageFormat format, std::unique_ptr<std::byte[]> imgPtr, int levels)
    : Image(format, levels) {
    data = std::move(imgPtr);
}

std::byte* Image::dataPtr(int lvl) const {
    std::size_t prevLvlSize =
        ImageSize({width, height, depth, numChan, compSize}, std::max(lvl - 1, 0));
    return &data.get()[prevLvlSize];
}

std::size_t Image::size(int level) const {
    int w = width * std::pow(0.5, level);
    int h = height * std::pow(0.5, level);
    int d = depth * std::pow(0.5, level);
    return w * std::max(h, 1) * std::max(d, 1) * compSize;
}

std::size_t Image::size() const {
    return ImageSize({width, height, depth, numChan, compSize}, levels);
}

void Image::copy(int dstX, int dstY, int srcX, int srcY, int lenX, int lenY,
                 const Image& src) {
    assert(src.compSize == compSize);
    assert(src.numChan == numChan);

    std::size_t nRows = lenY;
    std::size_t pxSize = pixelOffset();

    auto* srcPtr = &(src.data.get()[pxSize * src.width * srcY + pxSize * srcX]);
    auto* dstPtr = &(data.get()[pxSize * width * dstY + pxSize * dstX]);
    for (std::size_t r = 0; r < nRows; ++r) {
        std::memcpy(dstPtr, srcPtr, lenX * pxSize);
        srcPtr += pxSize * src.width;
        dstPtr += pxSize * width;
    }
}

void Image::copy(const RawImage& src) {
    // Gets rid of unwanted channels (e.g. alpha from tinyexr)

    assert(src.fmt.compSize == compSize);
    assert(src.fmt.width == width && src.fmt.height == height);

    std::size_t nPixels = width * std::max(height, 1) * std::max(depth, 1);
    std::size_t pxSizeDst = pixelOffset();
    std::size_t pxSizeSrc = src.fmt.compSize * src.fmt.numChannels;

    data = std::make_unique<std::byte[]>(size());

    auto* srcPtr = src.data;
    auto* dstPtr = data.get();
    for (std::size_t px = 0; px < nPixels; ++px) {
        std::memcpy(dstPtr, srcPtr, pxSizeDst);
        srcPtr += pxSizeSrc;
        dstPtr += pxSizeDst;
    }

    free(src.data);
}

std::size_t ibl::ImageSize(const ImageFormat& fmt, int levels) {
    return fmt.width * std::max(fmt.height, 1) * std::max(fmt.depth, 1) * fmt.compSize *
           fmt.numChannels * (std::pow(2, 2 * levels) - 1) / 3;
}