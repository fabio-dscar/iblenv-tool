#ifndef __IBL_UTIL_H__
#define __IBL_UTIL_H__

#include <string>
#include <filesystem>
#include <optional>
#include <glad/glad.h>

namespace ibl {
namespace util {

float* LoadEXRImage(const std::string& filePath);
bool SaveEXRImage(const std::string& fname, unsigned int width, unsigned int height,
                  unsigned int numChannels, float* data);

std::optional<std::string> ReadFile(const std::string& filePath,
                                    std::ios_base::openmode mode);

bool CheckOpenGLError();
void ExitWithError(const std::string& message);

void GLAPIENTRY OpenGLErrorCallback(GLenum source, GLenum type, GLuint id,
                                    GLenum severity, GLsizei length,
                                    const GLchar* message, const void* userParam);

} // namespace util
} // namespace ibl

#endif // __IBL_UTIL_H__