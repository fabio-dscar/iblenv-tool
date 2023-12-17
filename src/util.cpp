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
using namespace std::filesystem;
using namespace std::literals;

std::unique_ptr<Image> LoadImageDump(const std::string& filePath,
                                     const ImageFormat& fmt) {
    std::ifstream file(filePath, std::ios_base::in | std::ios_base::binary);
    if (file.fail())
        FATAL("Failed to open file {}", filePath);

    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    auto imgSize = ibl::ImageSize(fmt);
    if (size != static_cast<long>(imgSize))
        FATAL("Incompatible format specified for image {}", filePath);

    auto data = std::make_unique<std::byte[]>(size);
    file.read(reinterpret_cast<char*>(data.get()), size);

    return std::make_unique<Image>(fmt, data.get(), 1);
}

std::unique_ptr<Image> util::LoadImage(const std::string& filePath, ImageFormat* fmt) {
    auto ext = path(filePath).extension().string();
    if (ext == ".exr")
        return LoadEXRImage(filePath);
    else if (ext == ".hdr")
        return LoadHDRImage(filePath);
    else if (ext == ".png")
        return LoadPNGImage(filePath);
    else if (ext == ".bin") {
        if (!fmt)
            FATAL("ImageFormat is required for loading raw dumps.");

        return LoadImageDump(filePath, *fmt);
    }

    FATAL("Unsupported format {}", ext);
}

std::unique_ptr<Image> util::LoadPNGImage(const std::string& filePath) {
    ImageFormat srcFmt{.pFmt = PixelFormat::U8};
    auto* data = reinterpret_cast<std::byte*>(
        stbi_load(filePath.c_str(), &srcFmt.width, &srcFmt.height, &srcFmt.nChannels, 0));

    if (!data)
        FATAL("Failed to load PNG image: {}", filePath);

    Print("Loaded {}x{} image {}", srcFmt.width, srcFmt.height, filePath);

    ImageFormat dstFmt = srcFmt;
    dstFmt.pFmt = PixelFormat::F32;
    dstFmt.nChannels = 3; // Get rid of alpha if available on source image

    auto retImg = std::make_unique<Image>(dstFmt, Image{srcFmt, data});
    free(data);

    return retImg;
}

std::unique_ptr<Image> util::LoadHDRImage(const std::string& filePath) {
    ImageFormat srcFmt{.pFmt = PixelFormat::F32};
    auto* data =
        stbi_loadf(filePath.c_str(), &srcFmt.width, &srcFmt.height, &srcFmt.nChannels, 0);

    if (!data)
        FATAL("Failed to load HDR image: {}", filePath);

    Print("Loaded {}x{} image {}", srcFmt.width, srcFmt.height, filePath);

    ImageFormat dstFmt = srcFmt;
    dstFmt.nChannels = 3; // Get rid of alpha if available on source image

    auto retImg = std::make_unique<Image>(dstFmt, Image{srcFmt, data});
    free(data);

    return retImg;
}

std::unique_ptr<Image> util::LoadEXRImage(const std::string& filePath, bool keepAlpha) {
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
        FATAL("Failed to load EXR image {}: {}", filePath, errStr);
    }

    Print("Loaded {}x{} image {}", srcFmt.width, srcFmt.height, filePath);

    ImageFormat dstFmt = srcFmt;
    if (!keepAlpha) // Throw away the alpha on copy
        dstFmt.nChannels = 3;

    auto retImg = std::make_unique<Image>(dstFmt, Image{srcFmt, out});
    free(out);

    return retImg;
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

void SaveRawImage(const std::string& fname, const ImageSpan& image) {
    std::ofstream file(fname, std::ios_base::out | std::ios_base::binary);
    file.write(reinterpret_cast<const char*>(image.data()), image.size());

    auto imgFmt = image.format();
    Print("Saved raw image data successfully:\n\n{}\nDimensions: {}x{}\nPixel Format: "
          "{}\nSize: {} bytes\n",
          path(fname).filename().string(), imgFmt.width, imgFmt.height,
          GetPixelFormat(imgFmt), image.size());
}

struct ImgHeader {
    char id[4] = {'I', 'M', 'G', ' '};
    unsigned int fmt;
    unsigned int width;
    unsigned int height;
    unsigned int depth;
    unsigned int compSize;
    unsigned int numChannels;
    unsigned int totalSize;
    unsigned int levels;
};

void SaveImgFormatImage(const path& filePath, const ImageSpan& img) {
    const auto& [parent, fname, ext] = SplitFilePath(filePath);

    auto imgFmt = img.format();

    ImgHeader header;
    header.fmt = 0;
    header.width = imgFmt.width;
    header.height = imgFmt.height;
    header.depth = 0;
    header.compSize = ComponentSize(imgFmt.pFmt);
    header.numChannels = imgFmt.nChannels;
    header.totalSize = img.size();
    header.levels = img.numLevels();

    auto outName = std::format("{}{}", fname, ".img");
    std::ofstream file(parent / outName, std::ios_base::out | std::ios_base::binary);
    file.write((const char*)&header, sizeof(ImgHeader));
    file.write(reinterpret_cast<const char*>(img.data()), header.totalSize);
    file.close();
}

void util::SaveMipmappedImage(const path& filePath, const Image& image) {
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
        SaveImage(parent / NameOutput(lvl), ImageSpan{image, lvl});
}

void util::SaveImage(const std::string& fname, const ImageSpan& image) {
    auto ext = path(fname).extension().string();
    if (ext == ".exr")
        SaveEXRImage(fname, image);
    else if (ext == ".hdr")
        SaveHDRImage(fname, image);
    else if (ext == ".png")
        SavePNGImage(fname, image);
    else if (ext == ".bin")
        SaveRawImage(fname, image);
    else if (ext == ".img")
        SaveImgFormatImage(fname, image);
    else
        FATAL("Unsupported format {}", ext);
}

void util::SavePNGImage(const std::string& fname, const ImageSpan& image) {
    auto imgFmt = image.format();

    // Always output 3 channel 8 bit for .png
    ImageFormat newFmt = imgFmt;
    newFmt.pFmt = PixelFormat::U8;
    newFmt.nChannels = 3;

    Image newImg = image.convertTo(newFmt);

    stbi_write_png(fname.c_str(), newFmt.width, newFmt.height, newFmt.nChannels,
                   newImg.data(), 0);
}

void util::SaveHDRImage(const std::string& fname, const ImageSpan& image) {
    auto imgFmt = image.format();

    // Always output 3 channel 32 bit for .HDR
    ImageFormat newFmt = imgFmt;
    newFmt.pFmt = PixelFormat::F32;
    newFmt.nChannels = 3;

    Image newImg = image.convertTo(newFmt);

    stbi_write_hdr(fname.c_str(), newFmt.width, newFmt.height, newFmt.nChannels,
                   reinterpret_cast<const float*>(newImg.data()));

    Print("Saved HDR file {}", fname);
}

void util::SaveEXRImage(const std::string& fname, const ImageSpan& image) {
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
    header.channels = new EXRChannelInfo[header.num_channels];

    // Must be BGR order
    std::array channelNames{"B", "G", "R"};
    for (int c = 0; c < outNumChannels; ++c) {
        std::strncpy(header.channels[c].name, channelNames[c], 255);
        header.channels[c].name[std::strlen(channelNames[c])] = '\0';
    }

    auto pxFormat = compSize == 2 ? TINYEXR_PIXELTYPE_HALF : TINYEXR_PIXELTYPE_FLOAT;
    header.pixel_types = new int[header.num_channels];
    header.requested_pixel_types = new int[header.num_channels];
    for (int i = 0; i < header.num_channels; i++) {
        // Input type
        header.pixel_types[i] = pxFormat;

        // Output stored in EXR
        header.requested_pixel_types[i] = pxFormat;
    }

    const char* err;
    int ret = SaveEXRImageToFile(&exrImage, &header, fname.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        std::string errMsg = err;

        FreeEXRErrorMessage(err);
        delete[] header.channels;
        delete[] header.pixel_types;
        delete[] header.requested_pixel_types;

        FATAL("Error saving EXR image {}", errMsg);
    }

    Print("Saved EXR file {}", fname);

    delete[] header.channels;
    delete[] header.pixel_types;
    delete[] header.requested_pixel_types;
}

std::optional<std::string> util::ReadTextFile(const std::string& filepath,
                                              std::ios_base::openmode mode) {
    std::ifstream file(filepath, mode);
    if (file.fail()) {
        perror(filepath.c_str());
        return {};
    }

    std::string contents;
    file.seekg(0, std::ios::end);
    contents.reserve(static_cast<std::size_t>(file.tellg()));
    file.seekg(0, std::ios::beg);

    contents.assign((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());

    return contents;
}

void GLAPIENTRY util::OpenGLErrorCallback(GLenum, GLenum type, GLuint, GLenum severity,
                                          GLsizei, const GLchar* message, const void*) {
    std::cerr << std::format("[OPENGL] Type = 0x{:x}, Severity = 0x{:x}, Message = {}\n",
                             type, severity, message);
}