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

std::unique_ptr<Image> LoadImageDump(const std::string& filePath,
                                     const ImageFormat& fmt) {
    std::ifstream file(filePath, std::ios_base::in | std::ios_base::binary);
    if (file.fail()) {
        perror(filePath.c_str());
        return nullptr;
    }

    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    auto imgSize = ibl::ImageSize(fmt);
    if (size != static_cast<long>(imgSize)) {
        ExitWithError("Wrong format specified for image {}", filePath);
    }

    auto data = std::make_unique<std::byte[]>(size);
    file.read(reinterpret_cast<char*>(data.get()), size);

    return std::make_unique<Image>(fmt, std::move(data));
}

std::unique_ptr<Image> util::LoadImage(const std::string& filePath, ImageFormat* fmt) {
    auto ext = path(filePath).extension().string();
    if (ext == ".exr")
        return LoadEXRImage(filePath);
    else if (ext == ".hdr")
        return LoadHDRImage(filePath);
    else if (ext == ".bin") {
        if (!fmt)
            ExitWithError("ImageFormat is required for loading raw dumps.");

        return LoadImageDump(filePath, *fmt);
    }

    ExitWithError("Unsupported format {}", ext);

    return nullptr;
}

std::unique_ptr<Image> util::LoadHDRImage(const std::string& filePath) {
    RawImage src;
    auto* data = stbi_loadf(filePath.c_str(), &src.fmt.width, &src.fmt.height,
                            &src.fmt.numChannels, 0);

    src.fmt.compSize = sizeof(float);
    src.name = path(filePath).filename();
    src.data = reinterpret_cast<std::byte*>(data);

    return std::make_unique<Image>(src.fmt, src);
}

std::unique_ptr<Image> util::LoadEXRImage(const std::string& filePath, bool halfFloats,
                                          bool keepAlpha) {
    // TODO: Load half floats

    float* out = nullptr; // width * height * RGBA
    const char* err = nullptr;

    RawImage src;
    int ret = LoadEXR(&out, &src.fmt.width, &src.fmt.height, filePath.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        if (err) {
            std::string errStr = err;
            FreeEXRErrorMessage(err);
            ExitWithError("{}", errStr);
        }
    } else {
        std::cout << std::format("Loaded! {}x{}\n", src.fmt.width, src.fmt.height);
    }

    src.name = path(filePath).filename();
    src.fmt.compSize = sizeof(float);
    src.fmt.numChannels = 4;
    src.data = reinterpret_cast<std::byte*>(out);

    ImageFormat dstFmt = src.fmt;
    if (!keepAlpha) // Throw away the alpha on copy
        dstFmt.numChannels = 3;

    return std::make_unique<Image>(dstFmt, src);
}

void DumpRawImage(const std::string& fname, std::size_t size, const void* data) {
    std::ofstream file(fname, std::ios_base::out | std::ios_base::binary);
    file.write(reinterpret_cast<const char*>(data), size);
    file.close();
}

void util::SaveMipmappedImage(const std::string& fname, const Image& image) {
    // TODO
    for (int lvl = 0; image.levels; ++lvl) {
        SaveImage(fname, ImageSpan{image, lvl});
    }
}

void util::SaveImage(const std::string& fname, const ImageSpan& image) {
    auto ext = path(fname).extension().string();
    if (ext == ".exr")
        SaveEXRImage(fname, image);
    else if (ext == ".hdr")
        SaveHDRImage(fname, image);
    else if (ext == ".bin")
        DumpRawImage(fname, image.size(), image.data());
    else
        ExitWithError("Unsupported format {}", ext);
}

void util::SaveHDRImage(const std::string fname, const ImageSpan& image) {
    stbi_write_hdr(fname.c_str(), image.width, image.height, image.numChan,
                   reinterpret_cast<const float*>(image.data()));
}

bool util::SaveEXRImage(const std::string& fname, const ImageSpan& image) {
    int width = image.width;
    int height = image.height;
    int numChannels = 3;
    const char* data = reinterpret_cast<const char*>(image.data());

    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage exrImage;
    InitEXRImage(&exrImage);

    exrImage.num_channels = numChannels;

    std::vector<char> images[3];
    images[0].resize(width * height * image.compSize);
    images[1].resize(width * height * image.compSize);
    images[2].resize(width * height * image.compSize);

    for (int i = 0; i < width * height; i++) {
        for (int c = 0; c < numChannels; c++) {
            if (c < image.numChan)
                std::memcpy(
                    &images[c][i * image.compSize],
                    &data[image.compSize * image.numChan * i + c * image.compSize],
                    image.compSize);
            // images[c][i] = data[image.numChannels * i + c];
            else
                images[c][i * image.compSize] = 0;
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
            image.compSize == 2 ? TINYEXR_PIXELTYPE_HALF
                                : TINYEXR_PIXELTYPE_FLOAT; // pixel type of input image
        header.requested_pixel_types[i] =
            image.compSize == 2 ? TINYEXR_PIXELTYPE_HALF
                                : TINYEXR_PIXELTYPE_FLOAT; // pixel type of output image
        //  to be stored in .EXR
    }

    const char* err;
    int ret = SaveEXRImageToFile(&exrImage, &header, fname.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        fprintf(stderr, "Save EXR err: %s\n", err);
        return ret;
    }
    printf("Saved exr file. [ %s ] \n", fname.c_str());

    free(header.channels);
    free(header.pixel_types);
    free(header.requested_pixel_types);

    return true;
}

std::optional<std::string> util::ReadFile(const std::string& filepath,
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

void util::ExitWithErrorMsg(const std::string& message) {
    std::cerr << "[ERROR] " << message << "\n";
    std::cin.get();
    exit(EXIT_FAILURE);
}

bool util::CheckOpenGLError() {
    bool isError = false;
    GLenum errCode;
    while ((errCode = glGetError()) != GL_NO_ERROR) {
        isError = true;
        std::cerr << std::format("OpenGL Error: {}\n", errCode);
    }
    return isError;
}

void GLAPIENTRY util::OpenGLErrorCallback(GLenum source, GLenum type, GLuint id,
                                          GLenum severity, GLsizei length,
                                          const GLchar* message, const void* userParam) {
    fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity,
            message);
}