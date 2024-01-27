#ifndef IBL_TEXTURE_H
#define IBL_TEXTURE_H

#include <iblenv.h>
#include <image.h>

#include <glad/glad.h>

namespace ibl {

struct FormatInfo;

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

    unsigned int handle = 0;
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
    unsigned int target = 0;
};

inline int MaxMipLevel(int width, int height = 0, int depth = 0) {
    int dim = std::max(width, std::max(height, depth));
    return 1 + static_cast<int>(std::floor(std::log2(dim)));
}

} // namespace ibl

#endif