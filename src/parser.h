#ifndef __IBL_PARSER_H__
#define __IBL_PARSER_H__

#include <string>
#include <cubemap.h>

namespace ibl {

enum class Mode { Unknown, Brdf, Irradiance, Convert, Convolution };

struct CliOptions {
    Mode mode = Mode::Unknown;
    CubeLayoutType importType;
    CubeLayoutType exportType;
    std::string outFile;
    std::string inFile;
    unsigned int numSamples;
    int mipLevels;
    int texSize;
    bool multiScattering;
    bool divideLambertConstant;
    bool usePrefilteredIS;
    bool useHalf;
    bool isInputEquirect;
    bool flipUv;
};

CliOptions ParseArgs(int argc, char* argv[]);

} // namespace ibl

#endif // __IBL_PARSER_H__