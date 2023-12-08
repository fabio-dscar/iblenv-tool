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

int getFaceSide(CubeExportType type, int smallSide) {
    using enum CubeExportType;
    switch (type) {
    case Separate:
    case Sequence:
    case VerticalSequence:
        return smallSide;
    case VerticalCross:
    case HorizontalCross:
    case InvHorizontalCross:
        return smallSide / 3;
    default:
        return 0;
    }
}

struct FaceMapping {
    int width = 0, height = 0;
    std::map<int, std::pair<int, int>> mapping;
};

using MappingFunc = std::function<FaceMapping(int)>;
using enum CubeExportType;

template<CubeExportType T>
MappingFunc getMapping();

template<>
MappingFunc getMapping<Sequence>() {
    return [](int cubeSide) -> FaceMapping {
        return {.width = 6 * cubeSide,
                .height = cubeSide,
                .mapping{
                    {0, {0, 0}},
                    {1, {cubeSide, 0}},
                    {2, {2 * cubeSide, 0}},
                    {3, {3 * cubeSide, 0}},
                    {4, {4 * cubeSide, 0}},
                    {5, {5 * cubeSide, 0}},
                }};
    };
}

template<>
MappingFunc getMapping<VerticalSequence>() {
    return [](int cubeSide) -> FaceMapping {
        return {.width = cubeSide,
                .height = 6 * cubeSide,
                .mapping{
                    {0, {0, 0}},
                    {1, {0, cubeSide}},
                    {2, {0, 2 * cubeSide}},
                    {3, {0, 3 * cubeSide}},
                    {4, {0, 4 * cubeSide}},
                    {5, {0, 5 * cubeSide}},
                }};
    };
}

template<>
MappingFunc getMapping<HorizontalCross>() {
    return [](int cubeSide) -> FaceMapping {
        return {.width = 4 * cubeSide,
                .height = 3 * cubeSide,
                .mapping{
                    {0, {2 * cubeSide, cubeSide}},
                    {1, {0, cubeSide}},
                    {2, {cubeSide, 0}},
                    {3, {cubeSide, 2 * cubeSide}},
                    {4, {cubeSide, cubeSide}},
                    {5, {3 * cubeSide, cubeSide}},
                }};
    };
}

template<>
MappingFunc getMapping<InvHorizontalCross>() {
    return [](int cubeSide) -> FaceMapping {
        return {.width = 4 * cubeSide,
                .height = 3 * cubeSide,
                .mapping{
                    {0, {3 * cubeSide, cubeSide}},
                    {1, {cubeSide, cubeSide}},
                    {2, {2 * cubeSide, 0}},
                    {3, {2 * cubeSide, 2 * cubeSide}},
                    {4, {2 * cubeSide, cubeSide}},
                    {5, {0, cubeSide}},
                }};
    };
}

template<>
MappingFunc getMapping<VerticalCross>() {
    return [](int cubeSide) -> FaceMapping {
        return {.width = 3 * cubeSide,
                .height = 4 * cubeSide,
                .mapping{
                    {0, {cubeSide, cubeSide}},
                    {1, {cubeSide, 3 * cubeSide}},
                    {2, {cubeSide, 0}},
                    {3, {cubeSide, 2 * cubeSide}},
                    {4, {0, cubeSide}},
                    {5, {2 * cubeSide, cubeSide}},
                }};
    };
}

void ExportSeparate(const path& filePath, const Texture& cube) {
    auto parent = filePath.parent_path();
    std::string fname = filePath.filename().replace_extension("");
    std::string ext = filePath.extension();

    static const std::map<unsigned int, std::string> FaceNames{
        {0, "+X"}, {1, "-X"}, {2, "+Y"}, {3, "-Y"}, {4, "+Z"}, {5, "-Z"}};

    auto NameOutput = [&](int face, int lvl) -> auto {
        if (cube.levels > 1)
            return std::format("{}_{}_{}{}", fname, FaceNames.at(face), lvl, ext);
        return std::format("{}_{}{}", fname, FaceNames.at(face), ext);
    };

    for (int lvl = 0; lvl < cube.levels; ++lvl) {
        auto fmt = cube.imgFormat(lvl);

        for (int face = 0; face < 6; ++face) {
            auto data = cube.faceData(face, lvl);

            auto outName = NameOutput(face, lvl);
            util::SaveImage(parent / outName, {fmt, std::move(data)});
        }
    }
}

void ExportCombined(const path& filePath, MappingFunc mapFunc, const Texture& cube) {
    auto parent = filePath.parent_path();
    std::string fname = filePath.filename().replace_extension("");
    std::string ext = filePath.extension();

    auto NameOutput = [&](int lvl) -> auto {
        if (cube.levels > 1)
            return std::format("{}_{}{}", fname, lvl, ext);
        return filePath.filename().string();
    };

    for (int lvl = 0; lvl < cube.levels; ++lvl) {
        ImageFormat cubeFmt = cube.imgFormat(lvl);
        FaceMapping map = mapFunc(cubeFmt.width);
        Image crossImg{{.width = map.width,
                        .height = map.height,
                        .numChannels = cubeFmt.numChannels,
                        .compSize = cubeFmt.compSize}};

        for (const auto& [face, coords] : map.mapping) {
            auto& [x, y] = coords;

            auto data = cube.faceData(face, lvl);
            crossImg.copy(x, y, 0, 0, cubeFmt.width, cubeFmt.height,
                          {cubeFmt, std::move(data)});
        }

        auto outName = NameOutput(lvl);
        util::SaveImage(parent / outName, crossImg);
    }
}

void util::ExportCubemap(const std::string& filePath, CubeExportType type,
                         const Texture& cube) {
    using enum CubeExportType;

    switch (type) {
    case Separate:
        ExportSeparate(filePath, cube);
        break;
    case Sequence:
        ExportCombined(filePath, getMapping<Sequence>(), cube);
        break;
    case VerticalSequence:
        ExportCombined(filePath, getMapping<VerticalSequence>(), cube);
        break;
    case HorizontalCross:
        ExportCombined(filePath, getMapping<HorizontalCross>(), cube);
        break;
    case InvHorizontalCross:
        ExportCombined(filePath, getMapping<InvHorizontalCross>(), cube);
        break;
    case VerticalCross:
        ExportCombined(filePath, getMapping<VerticalCross>(), cube);
        break;
    default:
        break;
    };
}

auto ImportSeparate(const path& filePath, ImageFormat* fmt) {

    auto parent = filePath.parent_path();
    std::string fname = filePath.filename().replace_extension("");
    std::string ext = filePath.extension();

    static const std::map<unsigned int, std::string> FaceNames{
        {0, "+X"}, {1, "-X"}, {2, "+Y"}, {3, "-Y"}, {4, "+Z"}, {5, "-Z"}};

    auto NameInput = [&](int face) -> auto {
        return std::format("{}_{}{}", fname, FaceNames.at(face), ext);
    };

    std::unique_ptr<Texture> texture = nullptr;

    for (const auto& [face, name] : FaceNames) {
        auto inputName = NameInput(face);
        auto imgFace = LoadImage(filePath, fmt);
        auto cubeSide = imgFace->width;

        if (!texture) {
            texture = std::make_unique<Texture>(
                GL_TEXTURE_CUBE_MAP, GL_RGB32F, cubeSide, cubeSide,
                SamplerOpts{.minFilter = GL_LINEAR_MIPMAP_LINEAR}, 0,
                MaxMipLevel(cubeSide));
        }

        texture->uploadCubeFace(face, imgFace->dataPtr());
    }

    return texture;
}

auto ImportCombined(const std::string& filePath, CubeExportType type, MappingFunc mapFunc,
                    ImageFormat* fmt) {

    auto img = LoadImage(filePath, fmt);

    auto cubeSide = getFaceSide(type, std::min(img->width, img->height));
    ImageFormat faceFmt{cubeSide, cubeSide, img->numChan, img->compSize};

    auto texture = std::make_unique<Texture>(
        GL_TEXTURE_CUBE_MAP, GL_RGB32F, cubeSide, cubeSide,
        SamplerOpts{.minFilter = GL_LINEAR_MIPMAP_LINEAR}, 0, MaxMipLevel(cubeSide));

    FaceMapping map = mapFunc(cubeSide);
    for (const auto& [face, coords] : map.mapping) {
        auto& [x, y] = coords;

        Image imgFace{faceFmt};
        imgFace.copy(0, 0, x, y, faceFmt.width, faceFmt.height, *img);
        texture->uploadCubeFace(face, imgFace.dataPtr());
    }

    return texture;
}

std::unique_ptr<Texture> ImportCubeMap(const std::string& filePath, CubeExportType type,
                                       ImageFormat* reqFmt) {

    using enum CubeExportType;
    switch (type) {
    case Separate:
        return ImportSeparate(filePath, reqFmt);
    case Sequence:
        return ImportCombined(filePath, type, getMapping<Sequence>(), reqFmt);
    case VerticalSequence:
        return ImportCombined(filePath, type, getMapping<VerticalSequence>(), reqFmt);
    case HorizontalCross:
        return ImportCombined(filePath, type, getMapping<HorizontalCross>(), reqFmt);
    case InvHorizontalCross:
        return ImportCombined(filePath, type, getMapping<InvHorizontalCross>(), reqFmt);
    case VerticalCross:
        return ImportCombined(filePath, type, getMapping<VerticalCross>(), reqFmt);
    default:
        return nullptr;
    };
}

std::unique_ptr<Texture> util::LoadCubemap(const std::string& filePath,
                                           ImageFormat* fmt) {
    using enum CubeExportType;

    auto img = LoadImage(filePath, fmt);

    auto cubeSide = getFaceSide(VerticalSequence, std::min(img->width, img->height));

    auto texture = std::make_unique<Texture>(
        GL_TEXTURE_CUBE_MAP, GL_RGB32F, cubeSide, cubeSide,
        SamplerOpts{.minFilter = GL_LINEAR_MIPMAP_LINEAR}, 0, MaxMipLevel(cubeSide));

    for (int face = 0; face < 6; ++face) {
        Image imgFace{{.width = cubeSide,
                       .height = cubeSide,
                       .numChannels = fmt->numChannels,
                       .compSize = fmt->compSize}};

        imgFace.copy(0, 0, 0, cubeSide * face, cubeSide, cubeSide, *img);
        texture->uploadCubeFace(static_cast<CubemapFace>(face), imgFace.data.get());
    }

    return texture;
}

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

    return std::make_unique<Image>(src.fmt, 1, src);
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

    return std::make_unique<Image>(dstFmt, 1, src);
}

void DumpRawImage(const std::string& fname, std::size_t size, void* data) {
    std::ofstream file(fname, std::ios_base::out | std::ios_base::binary);
    file.write(reinterpret_cast<char*>(data), size);
    file.close();
}

void util::SaveImage(const std::string& fname, const Image& image) {
    auto ext = path(fname).extension().string();
    if (ext == ".exr")
        SaveEXRImage(fname, image);
    else if (ext == ".hdr")
        SaveHDRImage(fname, image);
    else if (ext == ".bin")
        DumpRawImage(fname, image.size(), image.data.get());
    else
        ExitWithError("Unsupported format {}", ext);
}

void util::SaveHDRImage(const std::string fname, const Image& image) {
    stbi_write_hdr(fname.c_str(), image.width, image.height, image.numChan,
                   reinterpret_cast<float*>(image.data.get()));
}

bool util::SaveEXRImage(const std::string& fname, const Image& image) {
    int width = image.width;
    int height = image.height;
    int numChannels = 3;
    char* data = reinterpret_cast<char*>(image.dataPtr());

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