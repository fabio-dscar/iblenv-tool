#include "util.h"
#include <parser.h>

#include <filesystem>

#include <argparse/argparse.hpp>

using namespace ibl;
using namespace std::filesystem;

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

    if (opts.mode == Mode::BRDF) {
        if (path(opts.outFile).extension() == ".hdr")
            util::ExitWithError("Exporting the brdf in .hdr format is not supported.");
    }
}

CliOptions BuildOptions(argparse::ArgumentParser& p) {
    CliOptions opts;

    auto& brdfMode = p.at<argparse::ArgumentParser>("brdf");
    if (p.is_subcommand_used(brdfMode)) {
        opts.mode = Mode::BRDF;
        opts.numSamples = brdfMode.get<unsigned int>("--spp");
        opts.texSize = brdfMode.get<int>("-s");
        opts.multiScattering = brdfMode.get<bool>("--ms");
        opts.outFile = brdfMode.get("-o");
        opts.useHalf = !brdfMode.get<bool>("--use32f");
        opts.flipUv = brdfMode.get<bool>("--flip-v");
        return opts;
    }

    auto& convertMode = p.at<argparse::ArgumentParser>("convert");
    if (p.is_subcommand_used(convertMode)) {
        opts.mode = Mode::CONVERT;
        opts.texSize = convertMode.get<int>("-s");
        opts.inFile = convertMode.get("-i");
        opts.outFile = convertMode.get("-o");
        opts.exportType = ibl::util::CubeExportType::VerticalSequence;
        return opts;
    }

    auto& irradianceMode = p.at<argparse::ArgumentParser>("irradiance");
    if (p.is_subcommand_used(irradianceMode)) {
        opts.mode = Mode::IRRADIANCE;
        opts.texSize = irradianceMode.get<int>("-s");
        opts.inFile = irradianceMode.get("-i");
        opts.outFile = irradianceMode.get("-o");
        opts.usePrefilteredIS = !irradianceMode.get<bool>("--no-prefiltered");
        opts.numSamples = irradianceMode.get<unsigned int>("--spp");
        opts.isInputEquirect = false; // irradianceMode.get("-t") == "equirect";
        opts.exportType = ibl::util::CubeExportType::VerticalSequence;
        return opts;
    }

    auto& convolutionMode = p.at<argparse::ArgumentParser>("convolution");
    if (p.is_subcommand_used(convolutionMode)) {
        opts.mode = Mode::CONVOLUTION;
        opts.texSize = convolutionMode.get<int>("-s");
        opts.inFile = convolutionMode.get("-i");
        opts.outFile = convolutionMode.get("-o");
        opts.usePrefilteredIS = !convolutionMode.get<bool>("--no-prefiltered");
        opts.numSamples = convolutionMode.get<unsigned int>("--spp");
        opts.mipLevels = convolutionMode.get<int>("-l");
        opts.isInputEquirect = false; // convolutionMode.get("-t") == "equirect";
        opts.exportType = ibl::util::CubeExportType::VerticalSequence;
        return opts;
    }

    return opts;
}

CliOptions ibl::ParseArgs(int argc, char* argv[]) {
    argparse::ArgumentParser program("iblenv", "1.0");
    program.add_description("ibl tool");

    argparse::ArgumentParser brdfCmd("brdf");
    brdfCmd.add_description("Computes microfacet brdf into a lookup table.");
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

    brdfCmd.add_argument("-s", "--texsize")
        .help("Specifies the number of samples per pixel.")
        .nargs(1)
        .default_value(1024)
        .scan<'i', int>();

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

    brdfCmd.add_argument("-o", "--outFile")
        .help("Filename for output file. By default dumps the bytes raw out of OpenGL "
              "('bin' extension).")
        .nargs(1)
        .default_value("brdf.bin");

    argparse::ArgumentParser convert("convert");
    convert.add_description("");
    convert.add_argument("-s", "--cubeSize")
        .help("Specifies the size of the cubemap.")
        .nargs(1)
        .default_value(1024)
        .scan<'i', int>();

    convert.add_argument("-i", "--input").help("Specifies the input file.").nargs(1);

    convert.add_argument("-o", "--outFile")
        .help("Output filename.")
        .nargs(1)
        .default_value("");

    argparse::ArgumentParser irradiance("irradiance");
    irradiance.add_description("");
    irradiance.add_argument("-s", "--cubeSize")
        .help("Specifies the size of the cubemap.")
        .nargs(1)
        .default_value(1024)
        .scan<'i', int>();

    irradiance.add_argument("-i", "--input").help("Specifies the input file.").nargs(1);

    irradiance.add_argument("-o", "--outFile")
        .help("Output filename.")
        .nargs(1)
        .default_value("");

    irradiance.add_argument("--no-prefiltered")
        .help("Disables prefiltered importance sampling.")
        .nargs(0)
        .default_value(false);

    irradiance.add_argument("--spp")
        .help("Specifies the number of samples per pixel.")
        .nargs(1)
        .default_value(2048u)
        .scan<'u', unsigned int>();

    irradiance.add_argument("-t", "--input-type")
        .help("Specifies the type of input.")
        .nargs(1)
        .default_value("equirect");

    argparse::ArgumentParser convolution("convolution");
    convolution.add_description("");
    convolution.add_argument("-s", "--cubeSize")
        .help("Specifies the size of the cubemap.")
        .nargs(1)
        .default_value(1024)
        .scan<'i', int>();
    convolution.add_argument("-l", "--levels")
        .help("Specifies the number of levels in the output cubemap.")
        .nargs(1)
        .default_value(9)
        .scan<'i', int>();

    convolution.add_argument("--no-prefiltered")
        .help("Disables prefiltered importance sampling.")
        .nargs(0)
        .default_value(false);
    convolution.add_argument("--spp")
        .help("Specifies the number of samples per pixel.")
        .nargs(1)
        .default_value(2048u)
        .scan<'u', unsigned int>();

    convolution.add_argument("-i", "--input").help("Specifies the input file.").nargs(1);

    convolution.add_argument("-t", "--input-type")
        .help("Specifies the type of input.")
        .nargs(1)
        .default_value("equirect");

    convolution.add_argument("-o", "--outFile")
        .help("Output filename.")
        .nargs(1)
        .default_value("");

    program.add_subparser(brdfCmd);
    program.add_subparser(convert);
    program.add_subparser(irradiance);
    program.add_subparser(convolution);

    program.parse_args(argc, argv);

    auto opts = BuildOptions(program);
    ValidateOptions(opts);

    return opts;
}
