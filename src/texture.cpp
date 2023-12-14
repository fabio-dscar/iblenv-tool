#include "glad/glad.h"
#include <texture.h>

#include <util.h>

using namespace ibl;

Texture::Texture(unsigned int target, unsigned int format, int width, int height,
                 SamplerOpts sampler, int layers, int levels)
    : width(width), height(height), levels(levels), target(target), layers(layers) {

    init(format, sampler);
}

Texture::Texture(const CubeImage& cube) {
    ImageFormat fmt = cube.imgFormat();

    target = GL_TEXTURE_CUBE_MAP;
    width = fmt.width;
    height = fmt.height;
    levels = MaxMipLevel(width);

    auto intFormat = DeduceIntFormat(fmt.compSize, fmt.numChannels);
    init(intFormat, {});

    upload(cube);
}

Texture::~Texture() {
    if (handle != 0)
        glDeleteTextures(1, &handle);
}

void Texture::init(unsigned int format, const SamplerOpts& sampler) {
    auto pair = TexFormatInfo.find(format);
    if (pair == TexFormatInfo.end())
        FATAL("Unsupported internal format {}", format);

    info = &pair->second;

    glCreateTextures(target, 1, &handle);

    if (target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP)
        glTextureStorage2D(handle, levels, info->intFormat, width, height);
    else if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
        glTextureStorage3D(handle, levels, info->intFormat, width, height, layers);

    glTextureParameteri(handle, GL_TEXTURE_WRAP_S, sampler.wrapS);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_T, sampler.wrapT);
    if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
        glTextureParameteri(handle, GL_TEXTURE_WRAP_R, sampler.wrapR);

    glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, sampler.minFilter);
    glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, sampler.magFilter);
}

void Texture::bind() const {
    glBindTexture(target, handle);
}

std::unique_ptr<std::byte[]> Texture::data(int level) const {
    auto size = sizeBytes(level);
    auto dataPtr = std::make_unique<std::byte[]>(size);
    glGetTextureImage(handle, level, info->format, info->type, size, dataPtr.get());
    return dataPtr;
}

std::unique_ptr<std::byte[]> Texture::data(int face, int level) const {
    auto size = sizeBytesFace(level);
    auto dataPtr = std::make_unique<std::byte[]>(size);
    bind();
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, info->format, info->type,
                  dataPtr.get());
    glBindTexture(target, 0);
    return dataPtr;
}

void Texture::upload(const ImageSpan& image, int lvl) const {
    glTextureSubImage2D(handle, lvl, 0, 0, image.width, image.height, info->format,
                        info->type, image.data());
}

void Texture::upload(const ImageSpan& image, int face, int lvl) const {
    glTextureSubImage3D(handle, lvl, 0, 0, face, width, height, 1, info->format,
                        info->type, image.data());
}

void Texture::upload(const CubeImage& cubemap) const {
    for (int lvl = 0; lvl < 1; ++lvl) {
        for (int faceIdx = 0; faceIdx < 6; ++faceIdx) {
            auto& cubeFace = cubemap.face(faceIdx);
            glTextureSubImage3D(handle, lvl, 0, 0, faceIdx, width, height, 1,
                                info->format, info->type, cubeFace.data(lvl));
        }
    }
}

std::unique_ptr<CubeImage> Texture::cubemap() const {
    auto imgFmt = imgFormat();
    imgFmt.levels = levels;

    auto cube = std::make_unique<CubeImage>(imgFmt);
    for (int faceIdx = 0; faceIdx < 6; ++faceIdx) {
        Image faceImg{imgFmt};
        for (int lvl = 0; lvl < levels; ++lvl)
            faceImg.copy(face(faceIdx, lvl), lvl);
        cube->setFace(faceIdx, std::move(faceImg));
    }
    return cube;
}

CubeImage Texture::cubemap(int level) const {
    CubeImage cube{imgFormat(level)};
    for (int faceIdx = 0; faceIdx < 6; ++faceIdx)
        cube.setFace(faceIdx, face(faceIdx, level));
    return cube;
}

std::size_t Texture::sizeBytes(unsigned int level) const {
    int faces = target == GL_TEXTURE_CUBE_MAP ? 6 : 1;
    int w = std::max(width >> level, 1);
    int h = std::max(height >> level, 1);
    return info->compSize * info->numChannels * w * h * std::max(1, layers) * faces;
}

std::size_t Texture::sizeBytesFace(unsigned int level) const {
    int w = std::max(width >> level, 1);
    int h = std::max(height >> level, 1);
    return info->compSize * info->numChannels * w * h;
}

ImageFormat Texture::imgFormat(int level) const {
    int w = width * std::pow(0.5, level);
    int h = height * std::pow(0.5, level);
    return {w, h, 0, info->numChannels, info->compSize};
}

void Texture::setParam(GLenum param, GLint val) const {
    glTextureParameteri(handle, param, val);
}

void Texture::generateMipmaps() const {
    glGenerateTextureMipmap(handle);
}