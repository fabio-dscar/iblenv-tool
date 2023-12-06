#ifndef __IBLCLI_H__
#define __IBLCLI_H__

#include <util.h>
#include <string>

namespace ibl {

class Texture;

enum class Mode { UNKNOWN, BRDF, IRRADIANCE, CONVERT };

struct ProgOptions {
    Mode mode = Mode::UNKNOWN;
    std::string outFile;
    std::string inFile;
    unsigned int numSamples;
    int texSize;
    bool multiScattering;
    bool divideLambertConstant;
    bool usePrefilteredIS;
};

enum UniformLocs {
    Projection = 0,
    View = 1,
    Model = 2,
    EnvMap = 3,
    NumSamples = 4,
    Roughness = 5,
};

ProgOptions ParseArgs(int argc, char* argv[]);

void InitOpenGL();
void ComputeBRDF(const ProgOptions& opts);
void ComputeIrradiance(const ProgOptions& opts);
void ComputeConvolution(const ProgOptions& opts);
void ConvertToCubemap(const ProgOptions& opts);
void Cleanup();

std::unique_ptr<Texture> SphericalProjToCubemap(const std::string& filePath, int cubeSize,
                                                float degs = 0.0f,
                                                bool swapHandedness = false);

inline void ExecuteJob(const ProgOptions& opts) {
    if (opts.mode == Mode::UNKNOWN)
        ibl::util::ExitWithError("Unknown option.");

    InitOpenGL();

    if (opts.mode == Mode::BRDF)
        ComputeBRDF(opts);
    else if (opts.mode == Mode::CONVERT)
        ConvertToCubemap(opts);
    else if (opts.mode == Mode::IRRADIANCE)
        ComputeConvolution(opts);

    Cleanup();
}

} // namespace ibl

#endif // __IBLCLI_H__