#ifndef __IBL_UTIL_H__
#define __IBL_UTIL_H__

#include <iblenv.h>

#include <optional>
#include <filesystem>

#include <glad/glad.h>

namespace fs = std::filesystem;

namespace ibl {

struct ImageFormat;
class Image;
class ImageView;

namespace util {

// ------------------------------------------------------------------
//    Images
// ------------------------------------------------------------------
std::unique_ptr<Image> LoadImage(const fs::path& filePath, ImageFormat* fmt = nullptr);

std::unique_ptr<Image> LoadHDRImage(const fs::path& filePath);
std::unique_ptr<Image> LoadEXRImage(const fs::path& filePath, bool keepAlpha = false);
std::unique_ptr<Image> LoadPNGImage(const fs::path& filePath);

void SaveImage(const fs::path& filePath, const ImageView& image);
void SaveMipmappedImage(const fs::path& filePath, const Image& image);

void SaveEXRImage(const fs::path& filePath, const ImageView& image);
void SaveHDRImage(const fs::path& filePath, const ImageView& image);
void SavePNGImage(const fs::path& filePath, const ImageView& image);

// ------------------------------------------------------------------
//    General IO
// ------------------------------------------------------------------
std::optional<std::string> ReadTextFile(const fs::path& filePath);

inline auto SplitFilePath(const fs::path& filePath) {
    auto parent = filePath.parent_path();
    std::string fname = filePath.filename().replace_extension("");
    std::string ext = filePath.extension();

    return std::tuple{parent, fname, ext};
}

// ------------------------------------------------------------------
//    Error handling
// ------------------------------------------------------------------
inline void PrintMsg(const std::string& message) {
    std::cout << "[INFO] " << message << '\n';
}

inline void PrintError(const std::string& message) {
    std::cerr << "[ERROR] " << message << '\n';
}

template<typename... Args>
void Print(const std::format_string<Args...>& fmt, Args&&... args) {
    PrintMsg(std::format(fmt, std::forward<Args>(args)...));
}

void GLAPIENTRY OpenGLErrorCallback(GLenum source, GLenum type, GLuint id,
                                    GLenum severity, GLsizei length,
                                    const GLchar* message, const void* userParam);

} // namespace util
} // namespace ibl

#endif // __IBL_UTIL_H__