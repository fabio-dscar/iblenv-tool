#ifndef __IBL_UTIL_H__
#define __IBL_UTIL_H__

#include <glad/glad.h>
#include <string>
#include <optional>
#include <format>
#include <memory>

namespace ibl {
namespace util {

struct RawImage {
    std::unique_ptr<std::byte> data = nullptr;
    int width;
    int height;
    int numChannels;
    unsigned int compSize;
};

RawImage LoadImage(const std::string& filePath);
RawImage LoadHDRImage(const std::string& filePath);
RawImage LoadEXRImage(const std::string& filePath, bool keepAlpha = false);

void SaveImage(const std::string& fname, std::size_t size, void* data);
bool SaveEXRImage(const std::string& fname, unsigned int width, unsigned int height,
                  unsigned int numChannels, float* data);

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