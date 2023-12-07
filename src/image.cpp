#include <image.h>

using namespace ibl;

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

std::size_t ibl::ImageSizeLevels(const ImageFormat& fmt, int levels) {
    return fmt.width * std::max(fmt.height, 1) * std::max(fmt.depth, 1) * fmt.compSize *
           fmt.numChannels * (std::pow(2, 2 * levels) - 1) / 3;
}