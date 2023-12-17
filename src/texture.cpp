#include "glad/glad.h"
#include <texture.h>

#include <util.h>

using namespace ibl;

Texture::Texture(unsigned int target, unsigned int format, int width, int height,
                 int levels)
    : width(width), height(height), levels(levels), target(target) {

    init(format);
}

Texture::Texture(const CubeImage& cube) {
    auto fmt = cube.imgFormat();

    target = GL_TEXTURE_CUBE_MAP;
    width = fmt.width;
    height = fmt.height;
    levels = MaxMipLevel(width);

    auto intFormat = DeduceIntFormat(ComponentSize(fmt.pFmt), fmt.nChannels);
    init(intFormat);

    upload(cube);
}

Texture::~Texture() {
    if (handle != 0)
        glDeleteTextures(1, &handle);
}

void Texture::init(unsigned int format) {
    auto pair = TexFormatInfo.find(format);
    if (pair == TexFormatInfo.end())
        FATAL("Unsupported internal format {}", format);

    info = &pair->second;

    glCreateTextures(target, 1, &handle);

    if (target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP)
        glTextureStorage2D(handle, levels, info->intFormat, width, height);

    glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (target == GL_TEXTURE_CUBE_MAP)
        glTextureParameteri(handle, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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

    glBindTexture(target, handle);
    glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, info->format, info->type,
                  dataPtr.get());
    glBindTexture(target, 0);

    return dataPtr;
}

void Texture::upload(const ImageSpan& image, int lvl) const {
    auto imgFmt = image.format(lvl);
    glTextureSubImage2D(handle, lvl, 0, 0, imgFmt.width, imgFmt.height, info->format,
                        info->type, image.data());
}

void Texture::upload(const CubeImage& cubemap) const {
    for (int lvl = 0; lvl < cubemap.numLevels(); ++lvl) {
        for (int face = 0; face < 6; ++face) {
            glTextureSubImage3D(handle, lvl, 0, 0, face, width, height, 1, info->format,
                                info->type, cubemap[face].data(lvl));
        }
    }
}

std::unique_ptr<Image> Texture::image(int level) const {
    return std::make_unique<Image>(imgFormat(level), data(level).get(), 1);
}

std::unique_ptr<CubeImage> Texture::cubemap() const {
    const auto fmt = imgFormat();

    auto cube = std::make_unique<CubeImage>(fmt, levels);

    for (int faceIdx = 0; faceIdx < 6; ++faceIdx) {
        Image faceImg{fmt, levels};
        for (int lvl = 0; lvl < levels; ++lvl)
            faceImg.copy(face(faceIdx, lvl), lvl);

        (*cube)[faceIdx] = std::move(faceImg);
    }

    return cube;
}

std::size_t Texture::sizeBytes(unsigned int level) const {
    int faces = target == GL_TEXTURE_CUBE_MAP ? 6 : 1;
    return sizeBytesFace(level) * faces;
}

std::size_t Texture::sizeBytesFace(unsigned int level) const {
    int w = ResizeLvl(width, level);
    int h = ResizeLvl(height, level);
    return ComponentSize(info->pxFmt) * info->numChannels * w * h;
}

ImageFormat Texture::imgFormat(int lvl) const {
    return {info->pxFmt, ResizeLvl(width, lvl), ResizeLvl(height, lvl),
            info->numChannels};
}

void Texture::setParam(GLenum param, GLint val) const {
    glTextureParameteri(handle, param, val);
}

void Texture::generateMipmaps() const {
    glGenerateTextureMipmap(handle);
}