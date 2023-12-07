#ifndef __IBL_UTIL_H__
#define __IBL_UTIL_H__

#include <glad/glad.h>
#include <string>
#include <optional>
#include <format>
#include <memory>

#include <image.h>
#include <texture.h>

namespace ibl {
namespace util {

std::unique_ptr<Image> LoadImage(const std::string& filePath);
std::unique_ptr<Image> LoadHDRImage(const std::string& filePath);
std::unique_ptr<Image> LoadEXRImage(const std::string& filePath, bool halfFloat = false,
                                    bool keepAlpha = false);

void SaveImage(const std::string& fname, const Image& image);
bool SaveEXRImage(const std::string& fname, const Image& image);
void SaveHDRImage(const std::string fname, const Image& image);

void ExportCubemap(const Texture& cube);

std::optional<std::string> ReadFile(const std::string& filePath,
                                    std::ios_base::openmode mode);

bool CheckOpenGLError();
void ExitWithErrorMsg(const std::string& message);

template<typename... Args>
void ExitWithError(const std::format_string<Args...>& fmt, Args&&... args) {
    ExitWithErrorMsg(std::format(fmt, std::forward<Args>(args)...));
}

void GLAPIENTRY OpenGLErrorCallback(GLenum source, GLenum type, GLuint id,
                                    GLenum severity, GLsizei length,
                                    const GLchar* message, const void* userParam);

} // namespace util
} // namespace ibl

#endif // __IBL_UTIL_H__