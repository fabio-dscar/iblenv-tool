#include <parser.h>

#include <argparse/argparse.hpp>

#include <filesystem>

using namespace ibl;
using namespace std::filesystem;

using namespace argparse;

void ParseSampledCube(const ArgumentParser& parser, CliOptions& opts) {
    opts.usePrefilteredIS = !parser.get<bool>("--no-prefiltered");
    opts.numSamples = parser.get<unsigned int>("--spp");
    opts.useHalf = parser.get<bool>("--use16f");
}

void ParseFileOpts(const ArgumentParser& parser, CliOptions& opts) {
    opts.inFile = parser.get("input");
    opts.outFile = parser.get("out");
    opts.texSize = parser.get<int>("-s");

    opts.isInputEquirect = !parser.is_used("--it");
    if (!opts.isInputEquirect)
        opts.importType = static_cast<CubeLayoutType>(parser.get<int>("--it"));
    opts.exportType = static_cast<CubeLayoutType>(parser.get<int>("--ot"));
}

CliOptions BuildOptions(ArgumentParser& p) {
    CliOptions opts;

    auto& brdf = p.at<ArgumentParser>("brdf");
    auto& convert = p.at<ArgumentParser>("convert");
    auto& irradiance = p.at<ArgumentParser>("irradiance");
    auto& specular = p.at<ArgumentParser>("specular");

    if (p.is_subcommand_used(brdf)) {
        opts.mode = Mode::Brdf;
        opts.outFile = brdf.get("out");
        opts.texSize = brdf.get<int>("-s");
        opts.numSamples = brdf.get<unsigned int>("--spp");
        opts.multiScattering = brdf.get<bool>("--ms");
        opts.useHalf = !brdf.get<bool>("--use32f");
        opts.flipUv = brdf.get<bool>("--flip-v");
        return opts;
    }

    if (p.is_subcommand_used(convert)) {
        opts.mode = Mode::Convert;
        ParseFileOpts(convert, opts);
        return opts;
    }

    if (p.is_subcommand_used(irradiance)) {
        opts.mode = Mode::Irradiance;
        ParseFileOpts(irradiance, opts);
        ParseSampledCube(irradiance, opts);
        opts.divideLambertConstant = irradiance.get<bool>("--div-pi");
        return opts;
    }

    if (p.is_subcommand_used(specular)) {
        opts.mode = Mode::Specular;
        ParseFileOpts(specular, opts);
        ParseSampledCube(specular, opts);
        opts.mipLevels = specular.get<int>("-l");
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
        .choices(0, 1, 2, 3, 4, 5, 6)
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
    sampled.add_argument("--use16f")
        .help("Computes result to 16 bit floats.")
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

    irradiance.add_argument("--div-pi")
        .help("Includes the lambertian constant division in the calculation.")
        .nargs(0)
        .implicit_value(true)
        .default_value(false);

    ArgumentParser specular("specular");
    specular.add_description("");
    specular.add_parents(inOut, sampled);

    specular.add_argument("-l", "--levels")
        .help("Specifies the number of levels in the output cubemap.")
        .nargs(1)
        .default_value(9)
        .scan<'i', int>();

    /* -------------------------------------- */

    program.add_subparser(brdfCmd);
    program.add_subparser(convert);
    program.add_subparser(irradiance);
    program.add_subparser(specular);

    program.parse_args(argc, argv);

    return BuildOptions(program);
}