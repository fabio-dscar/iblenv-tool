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

ProgOptions ParseArgs(int argc, char* argv[]);

void InitOpenGL();
void ComputeBRDF(const ProgOptions& opts);
void ComputeIrradiance(const ProgOptions& opts, const Texture& envMap);
void ConvertToCubemap(const ProgOptions& opts);
void Cleanup();

inline void ExecuteJob(const ProgOptions& opts) {
    if (opts.mode == Mode::UNKNOWN)
        ibl::util::ExitWithError("Unknown option.");

    InitOpenGL();

    if (opts.mode == Mode::BRDF)
        ComputeBRDF(opts);
    else if (opts.mode == Mode::CONVERT)
        ConvertToCubemap(opts);

    Cleanup();
}

} // namespace ibl

#endif // __IBLCLI_H__