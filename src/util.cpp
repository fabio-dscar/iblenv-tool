#include <cstdlib>
#include <ios>
#include <memory>
#include <util.h>

#include <fstream>
#include <iostream>
#include <filesystem>
#include <format>

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

using namespace ibl;
using namespace ibl::util;
using namespace std::filesystem;

RawImage util::LoadImage(const std::string& filePath) {
    auto ext = path(filePath).extension().string();
    if (ext == ".exr")
        return LoadEXRImage(filePath);
    else if (ext == ".hdr")
        return LoadHDRImage(filePath);

    ExitWithError("Unsupported format {}", ext);

    return {};
}

RawImage util::LoadHDRImage(const std::string& filePath) {
    // stbi_set_flip_vertically_on_load(true);
    int width, height, nChan;
    std::byte* data = reinterpret_cast<std::byte*>(
        stbi_loadf(filePath.c_str(), &width, &height, &nChan, 0));

    return {std::unique_ptr<std::byte>(data), width, height, nChan, sizeof(float)};
}

RawImage util::LoadEXRImage(const std::string& filePath, bool keepAlpha) {
    float* out = nullptr; // width * height * RGBA
    int width;
    int height;
    const char* err = nullptr; // or nullptr in C++11

    int ret = LoadEXR(&out, &width, &height, filePath.c_str(), &err);
    if (ret != TINYEXR_SUCCESS) {
        if (err) {
            std::string errStr = err;
            FreeEXRErrorMessage(err);
            ExitWithError("{}", errStr);
        }
    } else {
        std::cout << std::format("Loaded! {}x{}\n", width, height);
    }

    return {std::unique_ptr<std::byte>(reinterpret_cast<std::byte*>(out)), width, height,
            4, sizeof(float)};
}

void DumpRawImage(const std::string& fname, std::size_t size, void* data) {
    std::ofstream file(fname, std::ios_base::out | std::ios_base::binary);
    file.write(reinterpret_cast<char*>(data), size);
    file.close();
}

void util::SaveImage(const std::string& fname, std::size_t size, void* data) {
    auto ext = path(fname).extension().string();
    if (ext == ".exr")
        return;
    else if (ext == ".hdr")
        return;
    else if (ext == ".bin")
        DumpRawImage(fname, size, data);
    else
        ExitWithError("Unsupported format {}", ext);
}

bool util::SaveEXRImage(const std::string& fname, unsigned int width, unsigned int height,
                        unsigned int numChannels, float* data) {
    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage image;
    InitEXRImage(&image);

    image.num_channels = 3;

    std::vector<float> images[3];
    images[0].resize(width * height);
    images[1].resize(width * height);
    images[2].resize(width * height);

    for (unsigned int i = 0; i < width * height; i++) {
        images[0][i] = data[numChannels * i + 0];
        images[1][i] = data[numChannels * i + 1];
        // images[2][i] = 0;
        images[2][i] = data[numChannels * i + 2];
    }

    float* image_ptr[3];
    image_ptr[0] = &(images[2].at(0)); // B
    image_ptr[1] = &(images[1].at(0)); // G
    image_ptr[2] = &(images[0].at(0)); // R

    image.images = (unsigned char**)image_ptr;
    image.width = width;
    image.height = height;

    header.num_channels = 3;
    header.channels =
        (EXRChannelInfo*)malloc(sizeof(EXRChannelInfo) * header.num_channels);
    // Must be BGR(A) order, since most of EXR viewers expect this channel order.
    strncpy(header.channels[0].name, "B", 255);
    header.channels[0].name[strlen("B")] = '\0';
    strncpy(header.channels[1].name, "G", 255);
    header.channels[1].name[strlen("G")] = '\0';
    strncpy(header.channels[2].name, "R", 255);
    header.channels[2].name[strlen("R")] = '\0';

    header.pixel_types = (int*)malloc(sizeof(int) * header.num_channels);
    header.requested_pixel_types = (int*)malloc(sizeof(int) * header.num_channels);
    for (int i = 0; i < header.num_channels; i++) {
        header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // pixel type of input image
        header.requested_pixel_types[i] =
            TINYEXR_PIXELTYPE_HALF; // pixel type of output image to be stored in .EXR
    }

    const char* err;
    int ret = SaveEXRImageToFile(&image, &header, fname.c_str(), &err);
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
    std::cerr << "" << message << "\n";
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