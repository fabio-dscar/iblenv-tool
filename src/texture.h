#ifndef __IBL_TEXTURE_H__
#define __IBL_TEXTURE_H__

#include <glad/glad.h>
#include <map>
#include <memory>
#include <cmath>

namespace ibl {

enum CubemapFace {
    CUBE_X_POS = 0,
    CUBE_X_NEG = 1,
    CUBE_Y_POS = 2,
    CUBE_Y_NEG = 3,
    CUBE_Z_POS = 4,
    CUBE_Z_NEG = 5
};

struct SamplerOpts {
    GLuint wrapS = GL_CLAMP_TO_EDGE;
    GLuint wrapT = GL_CLAMP_TO_EDGE;
    GLuint wrapR = GL_CLAMP_TO_EDGE;
    GLuint magFilter = GL_LINEAR;
    GLuint minFilter = GL_LINEAR;
};

struct FormatInfo {
    unsigned int numChannels;
    unsigned int compSize;
    GLuint intFormat;
    GLuint format;
    GLuint type;
};

static const std::map<unsigned int, FormatInfo> TexFormatInfo{
    {GL_R16F, {1, sizeof(float), GL_R16F, GL_R, GL_FLOAT}},
    {GL_RG16F, {2, sizeof(float), GL_RG16F, GL_RG, GL_FLOAT}},
    {GL_RGB16F, {3, sizeof(float), GL_RGB16F, GL_RGB, GL_FLOAT}}};

class Texture {
public:
    Texture(unsigned int target, unsigned int format, int sideSize)
        : Texture(target, format, sideSize, sideSize, {}) {}
    Texture(unsigned int target, unsigned int format, int width, int height,
            SamplerOpts sampler, int layers = 0, int levels = 1);

    ~Texture();

    void generateMipmaps() const;
    void setParam(GLenum param, GLint val) const;

    void uploadData(void* dataPtr) const;
    void uploadCubeFace(CubemapFace face, void* dataPtr) const;

    std::unique_ptr<std::byte[]> getData(int level = 0) const;
    std::unique_ptr<std::byte[]> getData(CubemapFace face, int level = 0) const;
    
    std::size_t sizeBytes(unsigned int level = 0) const;
    std::size_t sizeBytesFace(unsigned int level = 0) const;

    GLuint target = 0;
    GLuint handle = 0;
    int width = 0, height = 0;
    int levels = 1, layers = 0;
    const FormatInfo* info = nullptr;
};

inline int MaxMipLevel(int width, int height = 0, int depth = 0) {
    int dim = std::max(width, std::max(height, depth));
    return 1 + static_cast<int>(std::floor(std::log2(dim)));
}

} // namespace ibl

#endif // __TEXTURE_H__