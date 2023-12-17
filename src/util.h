#ifndef __IBL_UTIL_H__
#define __IBL_UTIL_H__

#include <glad/glad.h>
#include <string>
#include <optional>
#include <format>
#include <memory>
#include <filesystem>

#include <image.h>
#include <texture.h>

#include <iostream>

#if defined(DEBUG)
#define THROW_ERROR(...)                                                                 \
    throw std::runtime_error(std::format("{} ({}): {}", std::string(__FILE__),           \
                                         std::to_string(__LINE__),                       \
                                         std::format(__VA_ARGS__)))
#else
#define THROW_ERROR(...) throw std::runtime_error(std::format(__VA_ARGS__))
#endif

#define FATAL(...) THROW_ERROR(__VA_ARGS__)

namespace ibl {
namespace util {

// --------------------------------------------
//    Images
//
std::unique_ptr<Image> LoadImage(const std::string& filePath, ImageFormat* fmt = nullptr);

std::unique_ptr<Image> LoadHDRImage(const std::string& filePath);
std::unique_ptr<Image> LoadEXRImage(const std::string& filePath, bool keepAlpha = false);
std::unique_ptr<Image> LoadPNGImage(const std::string& filePath);

void SaveImage(const std::string& fname, const ImageSpan& image);
void SaveMipmappedImage(const std::filesystem::path& filePath, const Image& image);

void SaveEXRImage(const std::string& fname, const ImageSpan& image);
void SaveHDRImage(const std::string& fname, const ImageSpan& image);
void SavePNGImage(const std::string& fname, const ImageSpan& image);

// --------------------------------------------
//    General IO
//
std::optional<std::string> ReadTextFile(const std::string& filePath,
                                        std::ios_base::openmode mode);

inline auto SplitFilePath(const std::filesystem::path& filePath) {
    auto parent = filePath.parent_path();
    std::string fname = filePath.filename().replace_extension("");
    std::string ext = filePath.extension();

    return std::tuple{parent, fname, ext};
}

// --------------------------------------------
//    Error handling
//
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