#ifndef IBL_UTIL_H
#define IBL_UTIL_H

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

void SaveImage(const fs::path& filePath, const ImageView& image);
void SaveMipmappedImage(const fs::path& filePath, const Image& image);

// ------------------------------------------------------------------
//    General IO
// ------------------------------------------------------------------
std::optional<std::string> ReadTextFile(const fs::path& filePath);

inline auto SplitFilePath(const fs::path& filePath) {
    auto parent = filePath.parent_path();
    std::string fname = filePath.filename().replace_extension("").string();
    std::string ext = filePath.extension().string();

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

#endif