#include "util.h"
#include <parser.h>

#include <filesystem>

#include <argparse/argparse.hpp>

using namespace ibl;
using namespace std::filesystem;

using namespace argparse;

bool IsSupportedFormat(const path& filePath) {
    const static std::vector formats{".exr", ".hdr", ".bin"};
    auto ext = filePath.extension();
    auto res = std::find(formats.begin(), formats.end(), ext);
    return res != formats.end() || filePath == "";
}

void ValidateOptions(const CliOptions& opts) {
    const static std::vector formats{".exr", ".hdr", ".bin"};

    if (!IsSupportedFormat(opts.inFile))
        util::ExitWithError("Input file {} has unsupported format.", opts.inFile);

    if (!IsSupportedFormat(opts.outFile))
        util::ExitWithError("Output file {} has unsupported format.", opts.outFile);

    if (!exists(opts.inFile))
        util::ExitWithError("Input file {} doesn't exist.", opts.inFile);

    if (opts.mode == Mode::Brdf) {
        if (path(opts.outFile).extension() == ".hdr")
            util::ExitWithError("Exporting the brdf in .hdr format is not supported.");
    }
}

CliOptions BuildOptions(ArgumentParser& p) {
    CliOptions opts;

    auto& brdfMode = p.at<ArgumentParser>("brdf");
    auto& convertMode = p.at<ArgumentParser>("convert");
    auto& irradianceMode = p.at<ArgumentParser>("irradiance");
    auto& convolutionMode = p.at<ArgumentParser>("convolution");

    bool usedBrdf = p.is_subcommand_used(brdfMode);
    bool usedConvert = p.is_subcommand_used(convertMode);
    bool usedIrr = p.is_subcommand_used(irradianceMode);
    bool usedConv = p.is_subcommand_used(convolutionMode);

    if (usedBrdf) {
        opts.mode = Mode::Brdf;
        opts.outFile = brdfMode.get("out");
        opts.texSize = brdfMode.get<int>("-s");
        opts.numSamples = brdfMode.get<unsigned int>("--spp");
        opts.multiScattering = brdfMode.get<bool>("--ms");
        opts.useHalf = !brdfMode.get<bool>("--use32f");
        opts.flipUv = brdfMode.get<bool>("--flip-v");
        return opts;
    }

    if (usedConvert) {
        opts.mode = Mode::Convert;
        opts.inFile = convertMode.get("input");
        opts.outFile = convertMode.get("out");
        opts.texSize = convertMode.get<int>("-s");

        auto inType = convertMode.get<int>("--it");
        opts.isInputEquirect = inType == -1;
        if (!opts.isInputEquirect)
            opts.importType = static_cast<CubeLayoutType>(inType);
        opts.exportType = static_cast<CubeLayoutType>(convertMode.get<int>("--ot"));
        return opts;
    }

    if (usedIrr) {
        opts.mode = Mode::Irradiance;
        opts.inFile = irradianceMode.get("input");
        opts.outFile = irradianceMode.get("out");
        opts.texSize = irradianceMode.get<int>("-s");

        auto inType = irradianceMode.get<int>("--it");
        opts.isInputEquirect = inType == -1;
        if (!opts.isInputEquirect)
            opts.importType = static_cast<CubeLayoutType>(inType);
        opts.exportType = static_cast<CubeLayoutType>(irradianceMode.get<int>("--ot"));

        opts.usePrefilteredIS = !irradianceMode.get<bool>("--no-prefiltered");
        opts.numSamples = irradianceMode.get<unsigned int>("--spp");
        return opts;
    }

    if (usedConv) {
        opts.mode = Mode::Convolution;
        opts.inFile = convolutionMode.get("input");
        opts.outFile = convolutionMode.get("out");
        opts.texSize = convolutionMode.get<int>("-s");

        auto inType = convolutionMode.get<int>("--it");
        opts.isInputEquirect = inType == -1;
        if (!opts.isInputEquirect)
            opts.importType = static_cast<CubeLayoutType>(inType);
        opts.exportType = static_cast<CubeLayoutType>(convolutionMode.get<int>("--ot"));

        opts.usePrefilteredIS = !convolutionMode.get<bool>("--no-prefiltered");
        opts.numSamples = convolutionMode.get<unsigned int>("--spp");
        opts.mipLevels = convolutionMode.get<int>("-l");
        return opts;
    }

    return opts;
}

CliOptions ibl::ParseArgs(int argc, char* argv[]) {
    /* --------------  Shared -------------- */
    ArgumentParser inOut("inout", "", default_arguments::none);
    inOut.add_argument("input").help("Specifies the input file.").nargs(1);
    inOut.add_argument("out").help("Output filename.").nargs(1);
    inOut.add_argument("--it")
        .help("Type of cubemap mapping for input file.")
        .nargs(1)
        .default_value(-1)
        .choices(-1, 0, 1, 2, 3, 4, 5, 6)
        .scan<'d', int>();
    inOut.add_argument("--ot")
        .help("Type of cubemap mapping for output file.")
        .nargs(1)
        .default_value(0)
        .choices(0, 1, 2, 3, 4, 5, 6)
        .scan<'d', int>();
    inOut.add_argument("-s", "--cubeSize")
        .help("Specifies the size of the cubemap.")
        .nargs(1)
        .default_value(1024)
        .scan<'d', int>();

    ArgumentParser sampled("sampled", "", default_arguments::none);
    sampled.add_argument("--no-prefiltered")
        .help("Disables prefiltered importance sampling.")
        .nargs(0)
        .implicit_value(true)
        .default_value(false);
    sampled.add_argument("--spp")
        .help("Specifies the number of samples per pixel.")
        .nargs(1)
        .default_value(2048u)
        .scan<'u', unsigned int>();

    /* --------------  Program -------------- */
    ArgumentParser program("iblenv", "1.0");
    program.add_description("ibl tool");

    ArgumentParser brdfCmd("brdf");
    brdfCmd.add_description("Computes microfacet brdf into a lookup texture.");
    brdfCmd.add_argument("out")
        .help("Filename for output file. By default dumps the bytes raw out of OpenGL "
              "('bin' extension).")
        .nargs(1)
        .default_value("brdf.bin");

    brdfCmd.add_argument("-s", "--texsize")
        .help("Specifies the width and height of the output texture.")
        .nargs(1)
        .default_value(1024)
        .scan<'i', int>();

    brdfCmd.add_argument("--spp")
        .help("Specifies the number of samples per pixel.")
        .nargs(1)
        .default_value(4096u)
        .scan<'u', unsigned int>();

    brdfCmd.add_argument("--use32f")
        .help("Compute brdf into a 32 bit float texture.")
        .nargs(0)
        .implicit_value(true)
        .default_value(false);

    brdfCmd.add_argument("--ms")
        .help("Apply multiscattering precomputation.")
        .nargs(0)
        .implicit_value(true)
        .default_value(false);

    brdfCmd.add_argument("--flip-v")
        .help("Flips v in UVs. By default outputs directly from OpenGL with uvs starting "
              "on bottom left.")
        .nargs(0)
        .implicit_value(true)
        .default_value(false);

    ArgumentParser convert("convert");
    convert.add_description("");
    convert.add_parents(inOut);

    ArgumentParser irradiance("irradiance");
    irradiance.add_description("");
    irradiance.add_parents(inOut, sampled);

    ArgumentParser convolution("convolution");
    convolution.add_description("");
    convolution.add_parents(inOut, sampled);

    convolution.add_argument("-l", "--levels")
        .help("Specifies the number of levels in the output cubemap.")
        .nargs(1)
        .default_value(9)
        .scan<'i', int>();

    /* -------------------------------------- */

    program.add_subparser(brdfCmd);
    program.add_subparser(convert);
    program.add_subparser(irradiance);
    program.add_subparser(convolution);

    program.parse_args(argc, argv);

    auto opts = BuildOptions(program);
    ValidateOptions(opts);

    return opts;
}
