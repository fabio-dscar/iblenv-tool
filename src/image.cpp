#include <image.h>

using namespace ibl;

Image::Image(const ImageFormat& format)
    : width(format.width), height(format.height), depth(format.depth),
      levels(format.levels), numChan(format.numChannels), compSize(format.compSize) {
    imgData = std::make_unique<std::byte[]>(size());
}

Image::Image(const ImageFormat& format, const Image& src, CopyExtents extents, int lvl)
    : Image(format) {
    copy(extents, src, lvl);
}

Image::Image(const ImageFormat& format, const RawImage& source) : Image(format) {
    copy(source);
}

/*Image::Image(const ImageFormat& format, void* srcData) : Image(format) {
    data = std::make_unique<std::byte[]>(size());
    std::memcpy(imgData.get(), srcData, size());
}*/

Image::Image(const ImageFormat& format, std::unique_ptr<std::byte[]> imgPtr)
    : width(format.width), height(format.height), depth(format.depth),
      levels(format.levels), numChan(format.numChannels), compSize(format.compSize) {
    imgData = std::move(imgPtr);
}

std::byte* Image::data(int lvl) const {
    auto fmt = format();
    fmt.levels = std::max(lvl - 1, 0);
    std::size_t prevLvlSize = ImageSize(fmt);
    return &imgData.get()[prevLvlSize];
}

std::byte* Image::pixel(int x, int y, int level) const {
    int widthLvl = width * std::pow(0.5, level);
    std::size_t offset = pixelSize() * (widthLvl * y + x);
    auto* ptrToLvl = data(level);
    return &ptrToLvl[offset];
}

std::size_t Image::size(int lvl) const {
    auto resizeFactor = std::pow(0.5, lvl);
    int w = width * resizeFactor;
    int h = height * resizeFactor;
    int d = depth * resizeFactor;
    return w * std::max(h, 1) * std::max(d, 1) * compSize * numChan;
}

std::size_t Image::size() const {
    std::size_t totalSize = 0;
    for (int l = 0; l < levels; ++l)
        totalSize += size(l);
    return totalSize;
}

void Image::copy(int dstX, int dstY, int srcX, int srcY, int lenX, int lenY, int toLevel,
                 const Image& src) {
    assert(src.compSize == compSize);
    assert(src.numChan == numChan);

    const std::size_t nRows = lenY;
    const std::size_t pxSize = pixelSize();

    auto* srcPtr = src.pixel(srcX, srcY);
    auto* dstPtr = pixel(dstX, dstY, toLevel);
    for (std::size_t r = 0; r < nRows; ++r) {
        std::memcpy(dstPtr, srcPtr, lenX * pxSize);
        srcPtr += pxSize * src.width;
        dstPtr += pxSize * width;
    }
}

void Image::copy(CopyExtents params, const Image& src, int toLevel) {
    copy(params.toX, params.toY, params.fromX, params.fromY, params.sizeX, params.sizeY,
         toLevel, src);
}

void Image::copy(const Image& src, int toLevel) {
    copy(0, 0, 0, 0, src.width, src.height, toLevel, src);
}

void Image::copy(const RawImage& src) {
    // Gets rid of unwanted channels (e.g. alpha from tinyexr)

    assert(src.fmt.compSize == compSize);
    assert(src.fmt.width == width && src.fmt.height == height);

    std::size_t nPixels = width * std::max(height, 1) * std::max(depth, 1);
    std::size_t pxSizeDst = pixelSize();
    std::size_t pxSizeSrc = src.fmt.compSize * src.fmt.numChannels;

    auto* srcPtr = src.data;
    auto* dstPtr = imgData.get();
    for (std::size_t px = 0; px < nPixels; ++px) {
        std::memcpy(dstPtr, srcPtr, pxSizeDst);
        srcPtr += pxSizeSrc;
        dstPtr += pxSizeDst;
    }

    free(src.data);
}

ImageFormat Image::format() const {
    return {width, height, depth, numChan, compSize, levels};
}

ImageFormat Image::format(int level) const {
    auto sizeFactor = std::pow(0.5, level);
    int w = width * sizeFactor;
    int h = height * sizeFactor;
    int d = depth * sizeFactor;
    return {w, h, d, numChan, compSize, 1};
}

ImageSpan::ImageSpan(const Image& image)
    : width(image.width), height(image.height), depth(image.depth), levels(image.levels),
      numChan(image.numChan), compSize(image.compSize) {
    start = image.data();
    spanSize = image.size();
}

ImageSpan::ImageSpan(const Image& image, int lvl) {
    start = image.data(lvl);
    spanSize = image.size(lvl);

    auto fmt = image.format(lvl);
    width = fmt.width;
    height = fmt.height;
    numChan = image.numChan;
    compSize = image.compSize;
    levels = 1;
}

ImageSpan::ImageSpan(const Image& image, SpanExtents extents) {
    start = image.pixel(extents.fromX, extents.fromY, extents.lvl);
    auto end = image.pixel(extents.fromX + extents.sizeX, extents.fromY + extents.sizeY,
                           extents.lvl);
    spanSize = end - start;

    width = extents.sizeX;
    height = extents.sizeY;
    numChan = image.numChan;
    compSize = image.compSize;
    levels = 1;
}

std::size_t ibl::ImageSize(const ImageFormat& fmt) {
    std::size_t totalSize = 0;

    auto sizeLvl = [&fmt](int lvl) {
        int w = fmt.width * std::pow(0.5, lvl);
        int h = fmt.height * std::pow(0.5, lvl);
        int d = fmt.depth * std::pow(0.5, lvl);
        return w * std::max(h, 1) * std::max(d, 1) * fmt.compSize * fmt.numChannels;
    };

    for (int l = 0; l < fmt.levels; ++l)
        totalSize += sizeLvl(l);

    return totalSize;
}