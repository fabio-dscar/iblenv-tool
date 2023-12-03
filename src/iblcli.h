#ifndef __IBLCLI_H__
#define __IBLCLI_H__

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <string>

namespace ibl {

inline GLFWwindow* window;

enum class Mode { UNKNOWN, BRDF, IRRADIANCE, CONVERT };
struct ProgOptions {
    Mode mode = Mode::UNKNOWN;
    std::string outFile;
    unsigned int numSamples;
    unsigned int texSize;
    bool multiScattering;
};

ProgOptions ParseArgs(int argc, char* argv[]);

void InitOpenGL();
void ComputeBRDF(const ProgOptions& opts);
void Cleanup();

inline void ExecuteJob(const ProgOptions& opts) {
    InitOpenGL();

    if (opts.mode == Mode::BRDF)
        ComputeBRDF(opts);

    Cleanup();
}

} // namespace ibl

#endif // __IBLCLI_H__