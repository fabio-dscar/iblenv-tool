#include "glad/glad.h"
#include "texture.h"
#include <util.h>

#include <fstream>
#include <iostream>
#include <filesystem>
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
    else if (ext == ".bin") {
        if (!fmt)
            FATAL("ImageFormat is required for loading raw dumps.");

        return LoadImageDump(filePath, *fmt);
    }

    FATAL("Unsupported format {}", ext);
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

    Image rawImg{srcFmt, data};
    free(data);

    return std::make_unique<Image>(dstFmt, rawImg);
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

    Image rawImg{srcFmt, out};
    free(out);

    return std::make_unique<Image>(dstFmt, rawImg);
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
    header.levels = img.levels;

    auto outName = std::format("{}{}", fname, ".img");
    std::ofstream file(parent / outName, std::ios_base::out | std::ios_base::binary);
    file.write((const char*)&header, sizeof(ImgHeader));
    file.write(reinterpret_cast<const char*>(img.data()), header.totalSize);
    file.close();
}

void util::SaveMipmappedImage(const path& filePath, const Image& image) {
    const auto& [parent, fname, ext] = SplitFilePath(filePath);

    auto NameOutput = [&](int lvl) -> auto {
        if (image.numLevels() > 1)
            return std::format("{}_{}{}", fname, lvl, ext);
        return filePath.filename().string();
    };

    Print("Saving {} mip levels...", image.numLevels());

    for (int lvl = 0; lvl < image.numLevels(); ++lvl)
        SaveImage(parent / NameOutput(lvl), ImageSpan{image, lvl});
}

void util::SaveImage(const std::string& fname, const ImageSpan& image) {
    auto ext = path(fname).extension().string();
    if (ext == ".exr")
        SaveEXRImage(fname, image);
    else if (ext == ".hdr")
        SaveHDRImage(fname, image);
    else if (ext == ".bin")
        SaveRawImage(fname, image);
    else if (ext == ".img")
        SaveImgFormatImage(fname, image);
    else
        FATAL("Unsupported format {}", ext);
}

void util::SaveHDRImage(const std::string& fname, const ImageSpan& image) {
    stbi_write_hdr(fname.c_str(), image.width(), image.height(), image.format().nChannels,
                   reinterpret_cast<const float*>(image.data()));

    Print("Saved HDR file {}", fname);
}

void util::SaveEXRImage(const std::string& fname, const ImageSpan& image) {
    int width = image.width();
    int height = image.height();
    int numChannels = 3;
    int compSize = ComponentSize(image.format().pFmt);
    const char* data = reinterpret_cast<const char*>(image.data());

    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage exrImage;
    InitEXRImage(&exrImage);

    exrImage.num_channels = numChannels;

    std::vector<char> images[3];
    images[0].resize(width * height * compSize);
    images[1].resize(width * height * compSize);
    images[2].resize(width * height * compSize);

    for (int i = 0; i < width * height; i++) {
        for (int c = 0; c < numChannels; c++) {
            if (c < image.format().nChannels)
                std::memcpy(&images[c][i * compSize],
                            &data[compSize * image.format().nChannels * i + c * compSize],
                            compSize);
            // images[c][i] = data[image.numChannels * i + c];
            else
                images[c][i * compSize] = 0;
        }
    }

    char* image_ptr[3];
    image_ptr[0] = reinterpret_cast<char*>(&(images[2].at(0))); // B
    image_ptr[1] = reinterpret_cast<char*>(&(images[1].at(0))); // G
    image_ptr[2] = reinterpret_cast<char*>(&(images[0].at(0))); // R

    exrImage.images = (unsigned char**)image_ptr;
    exrImage.width = width;
    exrImage.height = height;

    header.num_channels = numChannels;
    header.channels =
        (EXRChannelInfo*)malloc(sizeof(EXRChannelInfo) * header.num_channels);
    // Must be BGR(A) order, since most of EXR viewers expect this channel order.

    std::array channelNames{"B", "G", "R"};
    for (int c = 0; c < numChannels; ++c) {
        strncpy(header.channels[c].name, channelNames[c], 255);
        header.channels[c].name[strlen(channelNames[c])] = '\0';
    }

    header.pixel_types = (int*)malloc(sizeof(int) * header.num_channels);
    header.requested_pixel_types = (int*)malloc(sizeof(int) * header.num_channels);
    for (int i = 0; i < header.num_channels; i++) {
        header.pixel_types[i] =
            compSize == 2 ? TINYEXR_PIXELTYPE_HALF
                          : TINYEXR_PIXELTYPE_FLOAT; // pixel type of input image
        header.requested_pixel_types[i] =
            compSize == 2 ? TINYEXR_PIXELTYPE_HALF
                          : TINYEXR_PIXELTYPE_FLOAT; // pixel type of output image
        //  to be stored in .EXR
    }

    const char* err;
    int ret = SaveEXRImageToFile(&exrImage, &header, fname.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        std::string errMsg = err;
        FreeEXRErrorMessage(err);
        FATAL("Error saving EXR image {}", errMsg);
    }

    Print("Saved EXR file {}", fname);

    free(header.channels);
    free(header.pixel_types);
    free(header.requested_pixel_types);
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