#include <texture.h>

#include <util.h>

using namespace ibl;

Texture::Texture(unsigned int target, unsigned int format, int width, int height,
                 SamplerOpts sampler, int layers)
    : target(target), width(width), height(height), layers(layers) {

    auto pair = TexFormatInfo.find(format);
    if (pair == TexFormatInfo.end())
        util::ExitWithError("Unsupported internal format {}", format);

    info = &pair->second;

    glCreateTextures(target, 1, &handle);

    if (target == GL_TEXTURE_2D)
        glTextureStorage2D(handle, 1, info->intFormat, width, height);
    else if (target == GL_TEXTURE_CUBE_MAP_ARRAY)
        glTextureStorage3D(handle, 1, info->intFormat, width, height, layers);

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

std::unique_ptr<std::byte[]> Texture::getData() const {
    auto size = sizeBytes();
    auto dataPtr = std::make_unique<std::byte[]>(size);
    glGetTextureImage(handle, 0, info->format, info->type, size, dataPtr.get());
    return dataPtr;
}

void Texture::uploadData(void* dataPtr) const {
    glTextureSubImage2D(handle, 0, 0, 0, width, height, info->format, info->type,
                        dataPtr);
}

std::size_t Texture::sizeBytes() const {
    return info->compSize * info->numChannels * width * height * std::max(1, layers);
}
