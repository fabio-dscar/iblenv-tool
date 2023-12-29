#ifndef __IBL_TEXTURE_H__
#define __IBL_TEXTURE_H__

#include <iblenv.h>
#include <image.h>

#include <glad/glad.h>

namespace ibl {

struct FormatInfo {
    int numChannels;
    PixelFormat pxFmt;
    GLuint intFormat;
    GLuint format;
    GLuint type;
};

static const std::map<unsigned int, FormatInfo> TexFormatInfo{
    {GL_R8,      {1, PixelFormat::U8, GL_R8, GL_R, GL_UNSIGNED_BYTE}      },
    {GL_RG8,     {2, PixelFormat::U8, GL_RG8, GL_RG, GL_UNSIGNED_BYTE}    },
    {GL_RGB8,    {3, PixelFormat::U8, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE}  },
    {GL_RGBA8,   {4, PixelFormat::U8, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE}},
    {GL_R16F,    {1, PixelFormat::F16, GL_R16F, GL_R, GL_HALF_FLOAT}      },
    {GL_RG16F,   {2, PixelFormat::F16, GL_RG16F, GL_RG, GL_HALF_FLOAT}    },
    {GL_RGB16F,  {3, PixelFormat::F16, GL_RGB16F, GL_RGB, GL_HALF_FLOAT}  },
    {GL_RGBA16F, {4, PixelFormat::F16, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT}},
    {GL_R32F,    {1, PixelFormat::F32, GL_R32F, GL_R, GL_FLOAT}           },
    {GL_RG32F,   {2, PixelFormat::F32, GL_RG32F, GL_RG, GL_FLOAT}         },
    {GL_RGB32F,  {3, PixelFormat::F32, GL_RGB32F, GL_RGB, GL_FLOAT}       },
    {GL_RGBA32F, {4, PixelFormat::F32, GL_RGBA32F, GL_RGBA, GL_FLOAT}     },
};

class Texture {
public:
    Texture(unsigned int target, unsigned int format, int sideSize)
        : Texture(target, format, sideSize, sideSize, 1) {}
    Texture(unsigned int target, unsigned int format, int side, int levels)
        : Texture(target, format, side, side, levels) {}
    Texture(unsigned int target, unsigned int format, int width, int height, int levels);
    explicit Texture(const CubeImage& cube);

    ~Texture();

    void bind() const;

    void generateMipmaps() const;
    void setParam(GLenum param, GLint val) const;

    void upload(const ImageView& image, int lvl = 0) const;
    void upload(const CubeImage& cubemap) const;

    std::size_t sizeBytes(unsigned int level = 0) const;
    std::size_t sizeBytesFace(unsigned int level = 0) const;

    ImageFormat imgFormat(int level = 0) const;

    std::unique_ptr<Image> image(int level = 0) const;
    std::unique_ptr<CubeImage> cubemap() const;

    GLuint handle = 0;
    int width = 0, height = 0;
    int levels = 1;

private:
    void init(unsigned int format);

    std::unique_ptr<std::byte[]> data(int level) const;
    std::unique_ptr<std::byte[]> data(int face, int level) const;

    Image face(int face, int level = 0) const {
        return {imgFormat(level), data(face, level).get()};
    }

    const FormatInfo* info = nullptr;
    GLuint target = 0;
};

inline int MaxMipLevel(int width, int height = 0, int depth = 0) {
    int dim = std::max(width, std::max(height, depth));
    return 1 + static_cast<int>(std::floor(std::log2(dim)));
}

inline unsigned int DeduceIntFormat(int compSize, int numChannels) {
    static const std::array f16s{GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F};
    static const std::array f32s{GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F};
    static const std::array u8s{GL_R8, GL_RG8, GL_RGB8, GL_RGBA8};

    switch (compSize) {
    case 1:
        return u8s[numChannels - 1];
    case 2:
        return f16s[numChannels - 1];
    case 4:
        return f32s[numChannels - 1];
    default:
        return 0;
    };
}

} // namespace ibl

#endif // __IBL_TEXTURE_H__