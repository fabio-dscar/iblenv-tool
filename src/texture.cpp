#include "glad/glad.h"
#include <texture.h>

#include <util.h>

using namespace ibl;

Texture::Texture(unsigned int target, unsigned int format, int width, int height,
                 SamplerOpts sampler, int layers, int levels)
    : target(target), width(width), height(height), levels(levels), layers(layers) {

    auto pair = TexFormatInfo.find(format);
    if (pair == TexFormatInfo.end())
        util::ExitWithError("Unsupported internal format {}", format);

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

Texture::~Texture() {
    if (handle != 0)
        glDeleteTextures(1, &handle);
}

std::unique_ptr<std::byte[]> Texture::getData(int level) const {
    auto size = sizeBytes();
    auto dataPtr = std::make_unique<std::byte[]>(size);
    glGetTextureImage(handle, level, info->format, info->type, size, dataPtr.get());
    return dataPtr;
}

std::unique_ptr<std::byte[]> Texture::getData(CubemapFace face, int level) const {
    auto size = sizeBytes();
    auto dataPtr = std::make_unique<std::byte[]>(size);
    glGetTextureImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, info->format,
                      info->type, size, dataPtr.get());
    return dataPtr;
}

void Texture::uploadData(void* dataPtr) const {
    glTextureSubImage2D(handle, 0, 0, 0, width, height, info->format, info->type,
                        dataPtr);
}

void Texture::uploadCubeFace(CubemapFace face, void* dataPtr) const {
    glTextureSubImage3D(handle, 0, 0, 0, face, width, height, 0, info->format, info->type,
                        dataPtr);
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

void Texture::setParam(GLenum param, GLint val) const {
    glTextureParameteri(handle, param, val);
}

void Texture::generateMipmaps() const {
    glGenerateTextureMipmap(handle);
}