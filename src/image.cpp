#include <image.h>

using namespace ibl;

float EncodeU8(std::uint8_t u8) {
    return u8 / 255.0f;
}

std::uint8_t DecodeU8(float f32) {
    f32 = std::min(1.0f, std::max(0.0f, f32));
    return static_cast<uint8_t>(f32 * 255.0f + 0.5f);
}

int ibl::ComponentSize(PixelFormat pFmt) {
    switch (pFmt) {
    case PixelFormat::U8:
        return 1;
    case PixelFormat::F16:
        return 2;
    case PixelFormat::F32:
        return 4;
    default:
        FATAL("Unknown pixel format.");
    }
}

Image::Image(ImageFormat format, int levels) : fmt(format), levels(levels) {
    resizeBuffer();
}

Image::Image(ImageFormat format, const std::byte* imgPtr, int levels)
    : fmt(format), levels(levels) {

    auto numElems = TotalPixels(format, levels) * fmt.nChannels;
    auto compSize = ComponentSize(fmt.pFmt);

    resizeBuffer();

    std::copy(imgPtr, imgPtr + numElems * compSize, getPtr());
}

Image::Image(ImageFormat format, const float* imgPtr, int levels)
    : fmt(format), levels(levels) {

    auto numElems = TotalPixels(format, levels) * fmt.nChannels;

    p32.reserve(numElems);
    p32.assign(imgPtr, imgPtr + numElems);
}

Image::Image(ImageFormat format, Image&& srcImg) : fmt(format), levels(srcImg.levels) {
    auto srcFmt = srcImg.format();

    // Move in if equivalent formats, convert from srcImg otherwise
    if (fmt.pFmt == srcFmt.pFmt && fmt.nChannels == srcFmt.nChannels)
        *this = std::move(srcImg);
    else {
        resizeBuffer();
        for (int lvl = 0; lvl < levels; ++lvl)
            copy(srcImg, lvl);
    }
}

float Image::channel(int x, int y, int c, int lvl) const {
    if (c >= fmt.nChannels)
        return 0;

    auto offset = pixelOffset(x, y, lvl) * fmt.nChannels;

    switch (fmt.pFmt) {
    case PixelFormat::U8:
        return EncodeU8(p8[offset + c]);
    case PixelFormat::F16:
        return p16[offset + c];
    case PixelFormat::F32:
        return p32[offset + c];
    default:
        FATAL("Unknown pixel format.");
    }
}

void Image::setChannel(float val, int x, int y, int c, int lvl) {
    assert(c < fmt.nChannels);

    auto offset = pixelOffset(x, y, lvl) * fmt.nChannels;

    switch (fmt.pFmt) {
    case PixelFormat::U8:
        p8[offset + c] = DecodeU8(val);
        break;
    case PixelFormat::F16:
        p16[offset + c] = val;
        break;
    case PixelFormat::F32:
        p32[offset + c] = val;
        break;
    default:
        FATAL("Unknown pixel format.");
    }
}

Image::PixelVal Image::pixel(int x, int y, int lvl) const {
    auto chanVals = PixelVal{0.0f, 0.0f, 0.0f, 0.0f};
    for (int c = 0; c < fmt.nChannels; ++c)
        chanVals[c] = channel(x, y, c, lvl);
    return chanVals;
}

void Image::setPixel(const PixelVal& px, int x, int y, int lvl) {
    for (int c = 0; c < fmt.nChannels; ++c)
        setChannel(px[c], x, y, c, lvl);
}

void Image::copy(const Image& srcImg, int toLvl, int fromLvl) {
    auto lvlSizes = format(toLvl);
    copy({.sizeX = lvlSizes.width, .sizeY = lvlSizes.height}, srcImg, toLvl, fromLvl);
}

void Image::copy(Extents ext, const Image& srcImg, int toLvl, int fromLvl) {
    const auto toX = ext.toX, toY = ext.toY;
    const auto fromX = ext.fromX, fromY = ext.fromY;

    for (int y = 0; y < ext.sizeY; ++y) {
        for (int x = 0; x < ext.sizeX; ++x) {
            const auto& px = srcImg.pixel(fromX + x, fromY + y, fromLvl);
            setPixel(px, toX + x, toY + y, toLvl);
        }
    }
}

Image Image::convertTo(ImageFormat newFmt, int nLvls) const {
    if (fmt.pFmt == newFmt.pFmt && fmt.nChannels == newFmt.nChannels && nLvls == levels)
        return *this;

    Image newImg{newFmt, nLvls};
    for (int lvl = 0; lvl < nLvls; ++lvl)
        newImg.copy(*this, lvl);

    return newImg;
}

void Image::resizeBuffer() {
    auto numElems = TotalPixels(fmt, levels) * fmt.nChannels;
    switch (fmt.pFmt) {
    case PixelFormat::U8:
        p8.resize(numElems);
        break;
    case PixelFormat::F16:
        p16.resize(numElems);
        break;
    case PixelFormat::F32:
        p32.resize(numElems);
        break;
    default:
        FATAL("Unknown pixel format.");
    }
}

std::size_t Image::pixelOffset(int x, int y, int lvl) const {
    auto nPixels = TotalPixels(fmt, lvl);
    return nPixels + (y * ResizeLvl(fmt.width, lvl) + x);
}

const std::byte* Image::getPtr() const {
    using enum PixelFormat;
    switch (fmt.pFmt) {
    case U8:
        return reinterpret_cast<const std::byte*>(p8.data());
    case F16:
        return reinterpret_cast<const std::byte*>(p16.data());
    case F32:
        return reinterpret_cast<const std::byte*>(p32.data());
    default:
        FATAL("Unknown pixel format.");
    }
}

std::byte* Image::getPtr() {
    using enum PixelFormat;
    switch (fmt.pFmt) {
    case U8:
        return reinterpret_cast<std::byte*>(p8.data());
    case F16:
        return reinterpret_cast<std::byte*>(p16.data());
    case F32:
        return reinterpret_cast<std::byte*>(p32.data());
    default:
        FATAL("Unknown pixel format.");
    }
}

void Image::flipXY() {
    Image flipImg{format(), levels};

    for (int lvl = 0; lvl < levels; ++lvl) {
        const auto lvlFmt = format(lvl);

        for (int x = 0; x < lvlFmt.width; ++x) {
            for (int y = 0; y < lvlFmt.height; ++y) {
                auto px = pixel(lvlFmt.width - 1 - x, lvlFmt.height - 1 - y, lvl);
                flipImg.setPixel(px, x, y, lvl);
            }
        }
    }

    *this = std::move(flipImg);
}

ImageSpan::ImageSpan(const Image& image)
    : img(&image), start(image.data()), spanSize(image.size()),
      nLevels(image.numLevels()) {}

ImageSpan::ImageSpan(const Image& image, int lvl)
    : img(&image), start(image.data(lvl)), spanSize(image.size(lvl)), nLevels(1),
      viewLevel(lvl) {}

Image ImageSpan::convertTo(ImageFormat newFmt, int lvl) const {
    Image copyImg{newFmt, 1};
    copyImg.copy(*img, lvl, viewLevel + lvl);
    return copyImg;
}

std::unique_ptr<std::byte[]> ibl::ExtractChannel(const Image& image, int c, int lvl) {
    const auto imgFmt = image.format(lvl);
    if (c >= imgFmt.nChannels)
        FATAL("Specified channel number is higher than available channels.");

    auto chSize = image.size(lvl) / imgFmt.nChannels;
    auto compSize = ComponentSize(imgFmt.pFmt);
    auto nPixels = imgFmt.width * imgFmt.height;

    auto chData = std::make_unique<std::byte[]>(chSize);

    auto imgPtr = image.data(lvl);
    auto dstPtr = chData.get();

    for (int p = 0; p < nPixels; ++p) {
        std::memcpy(dstPtr, imgPtr + compSize * c, compSize);
        imgPtr += compSize * imgFmt.nChannels;
        dstPtr += compSize;
    }

    return chData;
}

std::unique_ptr<std::byte[]> ibl::ExtractChannel(const ImageSpan imgView, int c,
                                                 int lvl) {
    assert(imgView.numLevels() > lvl);
    return ExtractChannel(*imgView.image(), c, imgView.level() + lvl);
}