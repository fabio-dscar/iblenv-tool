#ifndef __IBL_TEXTURE_H__
#define __IBL_TEXTURE_H__

#include <glad/glad.h>
#include <map>
#include <memory>

namespace ibl {

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
            SamplerOpts sampler, int layers = 0);

    ~Texture();

    void uploadData(void* dataPtr) const;
    std::unique_ptr<std::byte[]> getData() const;
    std::size_t sizeBytes() const;

    GLuint target = 0;
    GLuint handle = 0;
    int width = 0, height = 0;
    int layers = 0;
    const FormatInfo* info = nullptr;
};

} // namespace ibl

#endif // __TEXTURE_H__