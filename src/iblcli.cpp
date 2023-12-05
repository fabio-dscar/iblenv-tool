#include <iblcli.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <shader.h>
#include <iostream>
#include <util.h>
#include <format>
#include <geometry.h>
#include <argparse/argparse.hpp>

#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/mat4x4.hpp>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <texture.h>
#include <framebuffer.h>

using namespace ibl;
using namespace std::literals;

GLFWwindow* window;

ProgOptions BuildOptions(argparse::ArgumentParser& p) {
    ProgOptions opts;

    auto& brdfMode = p.at<argparse::ArgumentParser>("brdf");
    if (p.is_subcommand_used(brdfMode)) {
        opts.mode = Mode::BRDF;
        opts.numSamples = brdfMode.get<unsigned int>("--spp");
        opts.texSize = brdfMode.get<int>("-s");
        opts.multiScattering = brdfMode.get<bool>("--ms");
        opts.outFile = brdfMode.get("-o");
        return opts;
    }

    auto& convertMode = p.at<argparse::ArgumentParser>("convert");
    if (p.is_subcommand_used(convertMode)) {
        opts.mode = Mode::CONVERT;
        opts.texSize = convertMode.get<int>("-s");
        opts.inFile = convertMode.get("-i");
        opts.outFile = convertMode.get("-o");
        return opts;
    }

    auto& irradianceMode = p.at<argparse::ArgumentParser>("irradiance");
    if (p.is_subcommand_used(irradianceMode)) {
        opts.mode = Mode::CONVERT;
        opts.texSize = irradianceMode.get<int>("-s");
        opts.inFile = irradianceMode.get("-i");
        opts.outFile = irradianceMode.get("-o");
        return opts;
    }

    return opts;
}

ProgOptions ibl::ParseArgs(int argc, char* argv[]) {
    argparse::ArgumentParser program("iblenv", "1.0");
    program.add_description("ibl tool");

    argparse::ArgumentParser brdfCmd("brdf");
    brdfCmd.add_description("");
    brdfCmd.add_argument("--spp")
        .help("Specifies the number of samples per pixel.")
        .nargs(1)
        .default_value(4096u)
        .scan<'u', unsigned int>();

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

    brdfCmd.add_argument("-o", "--outFile")
        .help("Filename for output file. By default dumps the bytes raw out of OpenGL "
              "('bin' extension).")
        .nargs(1)
        .default_value("brdf.bin");

    program.add_subparser(brdfCmd);

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

    program.add_subparser(convert);

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

    program.add_subparser(irradiance);

    argparse::ArgumentParser convolution("convolution");
    convolution.add_description("");
    convolution.add_argument("-l", "--levels")
        .help("Specifies the number of levels in the output cubemap.")
        .nargs(1)
        .default_value(9)
        .scan<'i', int>();

    convolution.add_argument("-i", "--input").help("Specifies the input file.").nargs(1);

    convolution.add_argument("-o", "--outFile")
        .help("Output filename.")
        .nargs(1)
        .default_value("");

    program.add_subparser(convolution);

    program.parse_args(argc, argv);

    return BuildOptions(program);
}

void ibl::InitOpenGL() {
    if (!glfwInit())
        util::ExitWithError("Couldn't initialize OpenGL context.");

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = glfwCreateWindow(640, 480, "Simple example", NULL, NULL);
    if (!window) {
        glfwTerminate();
        util::ExitWithError("Couldn't create GLFW window.");
    }
    glfwMakeContextCurrent(window);

    int glver = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    if (glver == 0)
        util::ExitWithError("Failed to initialize OpenGL context");

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(ibl::util::OpenGLErrorCallback, 0);
    glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 0, 0,
                          GL_FALSE);

    // Print system info
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
    std::cout << "OpenGL Renderer: " << renderer << " (" << vendor << ")\n";
    std::cout << "OpenGL version " << version << '\n';
    std::cout << "GLSL version " << glslVersion << "\n\n";

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDepthRange(0.0, 1.0);
    glClearDepth(1.0);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

void ibl::Cleanup() {
    CleanupGeometry();

    if (window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

void ibl::ComputeBRDF(const ProgOptions& opts) {
    auto defines = std::vector<std::string>{};
    if (opts.multiScattering)
        defines.emplace_back("MULTISCATTERING");

    auto shaders = std::vector{"brdf.vert"s, "brdf.frag"s};
    auto program = CompileAndLinkProgram("brdf", shaders, defines);

    Texture brdfLUT{GL_TEXTURE_2D, GL_RG16F, opts.texSize};

    Framebuffer fb{};
    fb.addDepthBuffer(opts.texSize, opts.texSize);
    fb.addTextureBuffer(GL_COLOR_ATTACHMENT0, brdfLUT);
    fb.bind();

    glViewport(0, 0, brdfLUT.width, brdfLUT.height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program->id());
    glUniform1i(1, opts.numSamples);

    RenderQuad();

    auto data = brdfLUT.getData();
    util::SaveImage(opts.outFile, brdfLUT.sizeBytes(), data.get());
    // util::SaveEXRImage(opts.outFile, opts.texSize, opts.texSize, 2,
    //                    reinterpret_cast<float*>(data.get()));
}

void ibl::ConvertToCubemap(const ProgOptions& opts) {
    int cubeSize = opts.texSize;

    auto shaders = std::vector{"convert.vert"s, "convert.frag"s};
    auto program = CompileAndLinkProgram("convert", shaders);

    auto img = util::LoadImage(opts.inFile);
    Texture rectMap{GL_TEXTURE_2D, GL_RGB16F, img.width, img.height, {}};
    rectMap.uploadData(img.data.get());

    Framebuffer fb{};
    fb.addDepthBuffer(opts.texSize, opts.texSize);
    fb.bind();

    Texture cubemap{GL_TEXTURE_CUBE_MAP_ARRAY,
                    GL_RGB16F,
                    opts.texSize,
                    opts.texSize,
                    {.minFilter = GL_LINEAR_MIPMAP_LINEAR},
                    6, // in texture cube array, layers = num faces
                    MaxMipLevel(opts.texSize)};

    glm::vec3 origin{0, 0, 0};
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 5.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(origin, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(origin, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(origin, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::lookAt(origin, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
        glm::lookAt(origin, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(origin, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

    glViewport(0, 0, opts.texSize, opts.texSize);
    glUseProgram(program->id());
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(captureProjection));
    glUniform1i(2, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rectMap.handle);
    glCullFace(GL_FRONT); // Skybox draws inner face
    for (int face = 0; face < 6; ++face) {
        glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(captureViews[face]));
        fb.addTextureLayer(GL_COLOR_ATTACHMENT0, cubemap, face);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        RenderCube();
    }
    glCullFace(GL_BACK);

    ComputeIrradiance(opts, cubemap);

    return;

    std::map<unsigned int, std::string> cubeMap{{0, "+X"}, {1, "-X"}, {2, "+Y"},
                                                {3, "-Y"}, {4, "+Z"}, {5, "-Z"}};

    auto dataOut = cubemap.getData();
    for (int i = 0; i < 6; i++)
        util::SaveEXRImage(std::format("{}.exr", cubeMap[i]), cubeSize, cubeSize, 3,
                           (float*)(&dataOut.get()[sizeof(float) * i * 3 * cubeSize *
                                                   cubeSize])); //&dataOut[5 * 3 *
                                                                // cubeSize * cubeSize]);

    auto superOut = std::make_unique<std::byte[]>(sizeof(float) * 3 * (4 * cubeSize) *
                                                  (3 * cubeSize));

    std::size_t sizePx = sizeof(float) * 3;
    // for (int i = 0; i < cubeSize; ++i) {

    std::size_t coordsX[] = {sizePx * (4 * cubeSize * cubeSize),
                             sizePx * (4 * cubeSize * cubeSize) + sizePx * cubeSize,
                             sizePx * (4 * cubeSize * cubeSize) + sizePx * 2 * cubeSize,
                             sizePx * (4 * cubeSize * cubeSize) + sizePx * 3 * cubeSize,
                             sizePx * cubeSize,
                             sizePx * (4 * cubeSize * 2 * cubeSize) + sizePx * cubeSize};
    std::size_t coordsSep[] = {sizePx * cubeSize * cubeSize,
                               4 * sizePx * cubeSize * cubeSize,
                               0,
                               5 * sizePx * cubeSize * cubeSize,
                               2 * sizePx * cubeSize * cubeSize,
                               3 * sizePx * cubeSize * cubeSize};

    for (int k = 0; k < 6; ++k) {
        for (int j = 0; j < cubeSize; ++j) {
            auto ptrDst = &superOut.get()[coordsX[k] + sizePx * (4 * cubeSize * j)];
            auto ptrSrc = &dataOut.get()[coordsSep[k] + sizePx * cubeSize * j];
            // auto ptrDst = &superOut.get()[sizePx * (4 * cubeSize * cubeSize) +
            // sizePx * (4 * cubeSize * j)]; auto ptrSrc = &dataOut.get()[sizePx *
            // cubeSize * cubeSize +
            //                             sizePx * cubeSize * (j - cubeSize)];
            std::memcpy(ptrDst, ptrSrc, sizePx * cubeSize);
        }
    }
    //}

    util::SaveEXRImage(std::format("mega.exr"), 4 * cubeSize, 3 * cubeSize, 3,
                       (float*)superOut.get());
}

void ibl::ComputeIrradiance(const ProgOptions& opts, const Texture& envMap) {
    auto defines = std::vector<std::string>{};
    if (opts.divideLambertConstant)
        defines.emplace_back("DIVIDE_PI");
    // if (opts.usePrefilteredIS)
    defines.emplace_back("PREFILTERED_IS");

    auto shaders = std::vector{"convert.vert"s, "convolution.frag"s};
    auto program = CompileAndLinkProgram("irradiance", shaders, defines);

    // Load input cubemap 'envmap'

    Framebuffer fb{};
    fb.addDepthBuffer(opts.texSize, opts.texSize);
    fb.bind();

    Texture irradiance{
        GL_TEXTURE_CUBE_MAP_ARRAY, GL_RGB16F, opts.texSize, opts.texSize, {}, 6};

    glm::vec3 origin{0, 0, 0};
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 5.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(origin, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(origin, glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(origin, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::lookAt(origin, glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
        glm::lookAt(origin, glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(origin, glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

    envMap.generateMipmaps();
    
    glUseProgram(program->id());
    glUniform1i(2, 0);
    glUniform1i(3, 2048);
    glUniform1f(4, 0.1f);
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(captureProjection));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, envMap.handle);
    glViewport(0, 0, opts.texSize, opts.texSize);
    glCullFace(GL_FRONT);
    for (int face = 0; face < 6; ++face) {
        glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(captureViews[face]));
        fb.addTextureLayer(GL_COLOR_ATTACHMENT0, irradiance, face);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        RenderCube();
    }
    glCullFace(GL_BACK);

    // Extract irradiance
    std::map<unsigned int, std::string> cubeMap{{0, "+X"}, {1, "-X"}, {2, "+Y"},
                                                {3, "-Y"}, {4, "+Z"}, {5, "-Z"}};

    auto dataOut = irradiance.getData();
    for (int i = 0; i < 6; i++)
        util::SaveEXRImage(
            std::format("{}.exr", cubeMap[i]), opts.texSize, opts.texSize, 3,
            (float*)(&dataOut
                          .get()[sizeof(float) * i * 3 * opts.texSize * opts.texSize]));

    auto superOut = std::make_unique<std::byte[]>(sizeof(float) * 3 * (4 * opts.texSize) *
                                                  (3 * opts.texSize));

    std::size_t sizePx = sizeof(float) * 3;
    // for (int i = 0; i < opts.texSize; ++i) {

    std::size_t coordsX[] = {
        sizePx * (4 * opts.texSize * opts.texSize),
        sizePx * (4 * opts.texSize * opts.texSize) + sizePx * opts.texSize,
        sizePx * (4 * opts.texSize * opts.texSize) + sizePx * 2 * opts.texSize,
        sizePx * (4 * opts.texSize * opts.texSize) + sizePx * 3 * opts.texSize,
        sizePx * opts.texSize,
        sizePx * (4 * opts.texSize * 2 * opts.texSize) + sizePx * opts.texSize};
    std::size_t coordsSep[] = {sizePx * opts.texSize * opts.texSize,
                               4 * sizePx * opts.texSize * opts.texSize,
                               0,
                               5 * sizePx * opts.texSize * opts.texSize,
                               2 * sizePx * opts.texSize * opts.texSize,
                               3 * sizePx * opts.texSize * opts.texSize};

    for (int k = 0; k < 6; ++k) {
        for (int j = 0; j < opts.texSize; ++j) {
            auto ptrDst = &superOut.get()[coordsX[k] + sizePx * (4 * opts.texSize * j)];
            auto ptrSrc = &dataOut.get()[coordsSep[k] + sizePx * opts.texSize * j];
            // auto ptrDst = &superOut.get()[sizePx * (4 * opts.texSize * opts.texSize) +
            // sizePx * (4 * opts.texSize * j)]; auto ptrSrc = &dataOut.get()[sizePx *
            // opts.texSize * opts.texSize +
            //                             sizePx * opts.texSize * (j - opts.texSize)];
            std::memcpy(ptrDst, ptrSrc, sizePx * opts.texSize);
        }
    }
    //}

    util::SaveEXRImage(std::format("mega.exr"), 4 * opts.texSize, 3 * opts.texSize, 3,
                       (float*)superOut.get());
}