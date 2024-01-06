#include <util.h>

#include <image.h>

#include <fstream>
#include <functional>
#include <unordered_map>

#define TINYEXR_USE_MINIZ 0
#include <zlib.h>
#define TINYEXR_IMPLEMENTATION
#include <tinyexr.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PSD
#define STBI_NO_BMP
#define STBI_NO_JPEG
#define STBI_NO_GIF
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

using namespace ibl;
using namespace ibl::util;
using namespace std::literals;

namespace fs = std::filesystem;

namespace {

std::unique_ptr<Image> LoadRawImage(const fs::path& filePath, const ImageFormat& fmt) {
    std::ifstream file(filePath,
                       std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
    if (file.fail())
        FATAL("Failed to open file {}", filePath.string());

    auto size = file.tellg();
    if (size == -1)
        FATAL("Failed to query file size of {}.", filePath.string());

    file.seekg(0, std::ios::beg);

    auto imgSize = ibl::ImageSize(fmt);
    if (size != static_cast<long>(imgSize))
        FATAL("Incompatible format specified for image {}", filePath.string());

    auto data = std::make_unique<std::byte[]>(size);
    file.read(reinterpret_cast<char*>(data.get()), size);

    return std::make_unique<Image>(fmt, data.get(), 1);
}

std::string GetPixelFormat(const ImageFormat& fmt) {
    static const std::array NumChannels{"R"s, "RG"s, "RGB"s};
    static const std::map<PixelFormat, std::string> PixelFormat{
        {PixelFormat::U8,  "8"  },
        {PixelFormat::F16, "16F"},
        {PixelFormat::F32, "32F"}
    };

    return std::format("{}{}", NumChannels[fmt.nChannels - 1], PixelFormat.at(fmt.pFmt));
}

void SaveRawImage(const fs::path& filePath, const ImageView& image) {
    std::ofstream file(filePath, std::ios_base::out | std::ios_base::binary);
    file.write(reinterpret_cast<const char*>(image.data()), image.size());

    auto imgFmt = image.format();
    Print("Saved raw image data successfully:\n\n{}\nDimensions: {}x{}\nPixel Format: "
          "{}\nSize: {} bytes\n",
          filePath.filename().string(), imgFmt.width, imgFmt.height,
          GetPixelFormat(imgFmt), image.size());
}

struct ImgHeader {
    std::uint8_t id[4] = {'I', 'M', 'G', ' '};
    std::uint32_t fmt;
    std::int32_t width;
    std::int32_t height;
    std::int32_t depth;
    std::uint32_t compSize;
    std::int32_t numChannels;
    std::uint32_t totalSize;
    std::uint32_t levels;
};

void SaveImgFormatImage(const fs::path& filePath, const ImageView& img) {
    const auto& [parent, fname, ext] = SplitFilePath(filePath);

    auto imgFmt = img.format();

    ImgHeader header;
    header.fmt = static_cast<std::uint32_t>(imgFmt.pFmt);
    header.width = imgFmt.width;
    header.height = imgFmt.height;
    header.depth = 0;
    header.compSize = ComponentSize(imgFmt.pFmt);
    header.numChannels = imgFmt.nChannels;
    header.totalSize = img.size();
    header.levels = img.numLevels();

    auto outName = std::format("{}{}", fname, ".img");
    std::ofstream file(parent / outName, std::ios_base::out | std::ios_base::binary);
    file.write(reinterpret_cast<const char*>(&header), sizeof(ImgHeader));
    file.write(reinterpret_cast<const char*>(img.data()), header.totalSize);
}

} // namespace

std::unique_ptr<Image> util::LoadImage(const fs::path& filePath, ImageFormat* fmt) {
    auto ext = filePath.extension().string();
    if (ext == ".exr")
        return LoadEXRImage(filePath);
    else if (ext == ".hdr")
        return LoadHDRImage(filePath);
    else if (ext == ".png")
        return LoadPNGImage(filePath);
    else if (ext == ".bin") {
        if (!fmt)
            FATAL("ImageFormat is required for loading raw dumps.");

        return LoadRawImage(filePath, *fmt);
    }

    FATAL("Unsupported format {}", ext);
}

std::unique_ptr<Image> util::LoadPNGImage(const fs::path& filePath) {
    ImageFormat srcFmt{.pFmt = PixelFormat::U8};
    auto* data = reinterpret_cast<std::byte*>(
        stbi_load(filePath.c_str(), &srcFmt.width, &srcFmt.height, &srcFmt.nChannels, 0));

    if (!data)
        FATAL("Failed to load PNG image: {}", filePath.string());

    Print("Loaded {}x{} image {}", srcFmt.width, srcFmt.height, filePath.string());

    ImageFormat dstFmt = srcFmt;
    dstFmt.pFmt = PixelFormat::F32;
    dstFmt.nChannels = 3; // Get rid of alpha if available on source image

    auto retImg = std::make_unique<Image>(dstFmt, Image{srcFmt, data});
    free(data);

    return retImg;
}

std::unique_ptr<Image> util::LoadHDRImage(const fs::path& filePath) {
    ImageFormat srcFmt{.pFmt = PixelFormat::F32};
    auto* data =
        stbi_loadf(filePath.c_str(), &srcFmt.width, &srcFmt.height, &srcFmt.nChannels, 0);

    if (!data)
        FATAL("Failed to load HDR image: {}", filePath.string());

    Print("Loaded {}x{} image {}", srcFmt.width, srcFmt.height, filePath.string());

    ImageFormat dstFmt = srcFmt;
    dstFmt.nChannels = 3; // Get rid of alpha if available on source image

    auto retImg = std::make_unique<Image>(dstFmt, Image{srcFmt, data});
    free(data);

    return retImg;
}

std::unique_ptr<Image> util::LoadEXRImage(const fs::path& filePath, bool keepAlpha) {
    float* out = nullptr; // width * height * RGBA
    const char* err = nullptr;

    ImageFormat srcFmt = {.pFmt = PixelFormat::F32, .nChannels = 4};

    int ret = LoadEXRWithLayer(&out, &srcFmt.width, &srcFmt.height, filePath.c_str(),
                               NULL, &err);

    if (ret != TINYEXR_SUCCESS) {
        std::string errStr = "";
        if (err) {
            errStr = err;
            FreeEXRErrorMessage(err);
        }
        FATAL("Failed to load EXR image {}: {}", filePath.string(), errStr);
    }

    Print("Loaded {}x{} image {}", srcFmt.width, srcFmt.height, filePath.string());

    ImageFormat dstFmt = srcFmt;
    if (!keepAlpha) // Throw away the alpha on copy
        dstFmt.nChannels = 3;

    auto retImg = std::make_unique<Image>(dstFmt, Image{srcFmt, out});
    free(out);

    return retImg;
}

void util::SaveMipmappedImage(const fs::path& filePath, const Image& image) {
    const auto& [parent, fname, ext] = SplitFilePath(filePath);

    auto numLevels = image.numLevels();

    auto NameOutput = [&](int lvl) -> auto {
        if (numLevels > 1)
            return std::format("{}_{}{}", fname, lvl, ext);
        return filePath.filename().string();
    };

    if (numLevels > 1)
        Print("Saving {} mip levels...", numLevels);

    for (int lvl = 0; lvl < image.numLevels(); ++lvl)
        SaveImage(parent / NameOutput(lvl), ImageView{image, lvl});
}

void util::SaveImage(const fs::path& filePath, const ImageView& image) {
    auto ext = filePath.extension().string();
    if (ext == ".exr")
        SaveEXRImage(filePath, image);
    else if (ext == ".hdr")
        SaveHDRImage(filePath, image);
    else if (ext == ".png")
        SavePNGImage(filePath, image);
    else if (ext == ".bin")
        SaveRawImage(filePath, image);
    else if (ext == ".img")
        SaveImgFormatImage(filePath, image);
    else
        FATAL("Unsupported format {}", ext);
}

void util::SavePNGImage(const fs::path& filePath, const ImageView& image) {
    auto imgFmt = image.format();

    // Always output 3 channel 8 bit for .png
    ImageFormat newFmt = imgFmt;
    newFmt.pFmt = PixelFormat::U8;
    newFmt.nChannels = 3;

    Image newImg = image.convertTo(newFmt);

    stbi_write_png(filePath.c_str(), newFmt.width, newFmt.height, newFmt.nChannels,
                   newImg.data(), 0);
}

void util::SaveHDRImage(const fs::path& filePath, const ImageView& image) {
    auto imgFmt = image.format();

    // Always output 3 channel 32 bit for .HDR
    ImageFormat newFmt = imgFmt;
    newFmt.pFmt = PixelFormat::F32;
    newFmt.nChannels = 3;

    Image newImg = image.convertTo(newFmt);

    stbi_write_hdr(filePath.c_str(), newFmt.width, newFmt.height, newFmt.nChannels,
                   reinterpret_cast<const float*>(newImg.data()));

    Print("Saved HDR file {}", filePath.string());
}

void util::SaveEXRImage(const fs::path& filePath, const ImageView& image) {
    using ByteArray = std::unique_ptr<std::byte[]>;

    constexpr int outNumChannels = 3; // Always save 3 channel image

    auto imgFmt = image.format();
    int nPixels = imgFmt.width * imgFmt.height;
    int compSize = ComponentSize(imgFmt.pFmt);

    // Allocate buffer set to zero if needed
    ByteArray zero = nullptr;
    if (imgFmt.nChannels < outNumChannels)
        zero = std::make_unique<std::byte[]>(compSize * nPixels);

    // Extract available channels
    ByteArray chData[outNumChannels];
    for (int c = 0; c < imgFmt.nChannels; ++c)
        chData[c] = ExtractChannel(image, c);

    /* ------------------------------------------------------------
            Setup output EXR
     -------------------------------------------------------------*/
    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage exrImage;
    InitEXRImage(&exrImage);

    // Set pointers in reverse order (BGR), which is what EXR viewers expect
    std::byte* image_ptr[outNumChannels] = {};
    for (int c = 0; c < imgFmt.nChannels; ++c)
        image_ptr[2 - c] = chData[c].get();

    // Fill remaining channels with zero (if any)
    for (int c = imgFmt.nChannels; c < outNumChannels; ++c)
        image_ptr[2 - c] = zero.get();

    exrImage.width = imgFmt.width;
    exrImage.height = imgFmt.height;
    exrImage.num_channels = outNumChannels;
    exrImage.images = (unsigned char**)image_ptr;

    header.num_channels = outNumChannels;

    auto channelInfo = std::make_unique<EXRChannelInfo[]>(header.num_channels);
    header.channels = channelInfo.get();

    // Must be BGR order
    std::array channelNames{"B", "G", "R"};
    for (int c = 0; c < outNumChannels; ++c) {
        std::strncpy(header.channels[c].name, channelNames[c], 255);
        header.channels[c].name[std::strlen(channelNames[c])] = '\0';
    }

    auto inPixelTypes = std::make_unique<int[]>(header.num_channels);
    auto outPixelTypes = std::make_unique<int[]>(header.num_channels);
    header.pixel_types = inPixelTypes.get();
    header.requested_pixel_types = outPixelTypes.get();

    auto pxFormat = compSize == 2 ? TINYEXR_PIXELTYPE_HALF : TINYEXR_PIXELTYPE_FLOAT;
    for (int i = 0; i < header.num_channels; i++) {
        header.pixel_types[i] = pxFormat;           // Input type
        header.requested_pixel_types[i] = pxFormat; // Output stored in EXR
    }

    const char* err;
    int ret = SaveEXRImageToFile(&exrImage, &header, filePath.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        std::string errMsg = err;
        FreeEXRErrorMessage(err);
        FATAL("Error saving EXR image {}", errMsg);
    }

    Print("Saved EXR file {}", filePath.string());
}

std::optional<std::string> util::ReadTextFile(const fs::path& filePath) {
    std::ifstream file(filePath, std::ios_base::in | std::ios_base::ate);
    if (file.fail()) {
        PrintError(std::format("Failed to open file", filePath.string()));
        return std::nullopt;
    }

    auto size = file.tellg();
    if (size == -1) {
        PrintError(std::format("Failed to query file size of {}.", filePath.string()));
        return std::nullopt;
    }

    file.seekg(0, std::ios::beg);

    std::string contents;
    contents.reserve(size);
    contents.assign((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());

    return contents;
}

void GLAPIENTRY util::OpenGLErrorCallback(GLenum, GLenum type, GLuint, GLenum severity,
                                          GLsizei, const GLchar* message, const void*) {
    std::cerr << std::format("[OPENGL] Type = 0x{:x}, Severity = 0x{:x}, Message = {}\n",
                             type, severity, message);
}