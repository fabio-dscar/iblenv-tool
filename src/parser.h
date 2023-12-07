#ifndef __IBL_PARSER_H__
#define __IBL_PARSER_H__

#include <string>

namespace ibl {

enum class Mode { UNKNOWN, BRDF, IRRADIANCE, CONVERT, CONVOLUTION };

struct CliOptions {
    Mode mode = Mode::UNKNOWN;
    std::string outFile;
    std::string inFile;
    unsigned int numSamples;
    int mipLevels;
    int texSize;
    bool multiScattering;
    bool divideLambertConstant;
    bool usePrefilteredIS;
    bool useHalfFloat;
    bool isInputEquirect;
    bool flipUv;
};

CliOptions ParseArgs(int argc, char* argv[]);

}

#endif // __IBL_PARSER_H__