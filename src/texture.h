#ifndef __IBL_TEXTURE_H__
#define __IBL_TEXTURE_H__

#include "image.h"
#include <glad/glad.h>
#include <map>
#include <memory>
#include <cmath>
#include <vector>

namespace ibl {

struct SamplerOpts {
    GLuint wrapS = GL_CLAMP_TO_EDGE;
    GLuint wrapT = GL_CLAMP_TO_EDGE;
    GLuint wrapR = GL_CLAMP_TO_EDGE;
    GLuint magFilter = GL_LINEAR;
    GLuint minFilter = GL_LINEAR;
};

struct FormatInfo {
    int numChannels;
    int compSize;
    GLuint intFormat;
    GLuint format;
    GLuint type;
};

static const std::map<unsigned int, FormatInfo> TexFormatInfo{
    {GL_R16F, {1, 2, GL_R16F, GL_R, GL_HALF_FLOAT}},
    {GL_RG16F, {2, 2, GL_RG16F, GL_RG, GL_HALF_FLOAT}},
    {GL_RGB16F, {3, 2, GL_RGB16F, GL_RGB, GL_HALF_FLOAT}},
    {GL_R32F, {1, 4, GL_R32F, GL_R, GL_FLOAT}},
    {GL_RG32F, {2, 4, GL_RG32F, GL_RG, GL_FLOAT}},
    {GL_RGB32F, {3, 4, GL_RGB32F, GL_RGB, GL_FLOAT}}};

class Texture {
public:
    Texture(unsigned int target, unsigned int format, int sideSize)
        : Texture(target, format, sideSize, sideSize, {}) {}
    Texture(unsigned int target, unsigned int format, unsigned int side, int levels = 1)
        : Texture(target, format, side, side, {}, 0, levels) {}
    Texture(unsigned int target, unsigned int format, int width, int height,
            SamplerOpts sampler, int layers = 0, int levels = 1);

    ~Texture();

    void bind() const;

    void generateMipmaps() const;
    void setParam(GLenum param, GLint val) const;

    void upload(const ImageSpan& image, int lvl = 0) const;
    void upload(const ImageSpan& image, int face, int lvl) const;

    std::size_t sizeBytes(unsigned int level = 0) const;
    std::size_t sizeBytesFace(unsigned int level = 0) const;

    ImageFormat imgFormat(int level = 0) const;

    Image image(int level = 0) const { 
        return {imgFormat(level), data(levels)}; 
    }

    Image face(int face, int level = 0) const {
        return {imgFormat(level), data(face, level)};
    }

    GLuint handle = 0;
    int width = 0, height = 0;
    int levels = 1;

private:
    std::unique_ptr<std::byte[]> data(int level) const;
    std::unique_ptr<std::byte[]> data(int face, int level) const;

    const FormatInfo* info = nullptr;
    GLuint target = 0;
    int layers = 0;
};

inline int MaxMipLevel(int width, int height = 0, int depth = 0) {
    int dim = std::max(width, std::max(height, depth));
    return 1 + static_cast<int>(std::floor(std::log2(dim)));
}

} // namespace ibl

#endif // __IBL_TEXTURE_H__