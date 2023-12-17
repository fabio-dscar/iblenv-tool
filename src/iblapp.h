#ifndef __IBLCLI_H__
#define __IBLCLI_H__

#include <parser.h>

#include <util.h>
#include <string>

namespace ibl {

class Texture;

enum UniformLocs {
    Projection = 0,
    View = 1,
    Model = 2,
    EnvMap = 3,
    NumSamples = 4,
    Roughness = 5,
};

std::vector<std::string> GetShaderDefines(const CliOptions& opts);

void InitOpenGL();
void ComputeBRDF(const CliOptions& opts);
void ComputeConvolution(const CliOptions& opts);
void ComputeIrradiance(const CliOptions& opts);
void ConvertToCubemap(const CliOptions& opts);
void Cleanup();

std::unique_ptr<Texture> SphericalProjToCubemap(const std::string& filePath, int cubeSize,
                                                float degs = 0.0f,
                                                bool swapHandedness = false);

inline void ExecuteJob(const CliOptions& opts) {
    if (opts.mode == Mode::Unknown)
        FATAL("Unknown option.");

    InitOpenGL();

    if (opts.mode == Mode::Brdf)
        ComputeBRDF(opts);
    else if (opts.mode == Mode::Convert)
        ConvertToCubemap(opts);
    else if (opts.mode == Mode::Irradiance)
        ComputeIrradiance(opts);
    else if (opts.mode == Mode::Convolution)
        ComputeConvolution(opts);

    Cleanup();
}

} // namespace ibl

#endif // __IBLCLI_H__