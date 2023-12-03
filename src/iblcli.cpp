#include <iblcli.h>

#include <cstdlib>
#include <shader.h>
#include <iostream>
#include <util.h>
#include <format>
#include <geometry.h>
#include <argparse/argparse.hpp>

using namespace ibl;
using namespace std::literals;

ProgOptions BuildOptions(argparse::ArgumentParser& p) {
    ProgOptions opts;

    auto& brdfMode = p.at<argparse::ArgumentParser>("brdf");
    if (p.is_subcommand_used(brdfMode)) {
        opts.mode = Mode::BRDF;
        opts.numSamples = brdfMode.get<unsigned int>("--spp");
        opts.texSize = brdfMode.get<unsigned int>("-s");
        opts.multiScattering = brdfMode.get<bool>("--ms");
        opts.outFile = brdfMode.get("-o");
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
        .default_value(1024u)
        .scan<'u', unsigned int>();

    brdfCmd.add_argument("--ms")
        .help("Apply multiscattering precomputation.")
        .nargs(0)
        .implicit_value(true)
        .default_value(false);

    brdfCmd.add_argument("-o", "--outFile")
        .help("Filename for output exr file.")
        .nargs(1)
        .default_value("brdf.exr");

    program.add_subparser(brdfCmd);

    program.parse_args(argc, argv);

    return BuildOptions(program);
}

void ibl::InitOpenGL() {
    if (!glfwInit())
        util::ExitWithError("[ERROR] Couldn't initialize OpenGL context.\n");

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = glfwCreateWindow(640, 480, "Simple example", NULL, NULL);
    if (!window) {
        glfwTerminate();
        util::ExitWithError("[ERROR] Couldn't create GLFW window.\n");
    }
    glfwMakeContextCurrent(window);

    int glver = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    if (glver == 0)
        util::ExitWithError("[ERROR] Failed to initialize OpenGL context\n");

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

    // Initialize OpenGL state
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
    if (window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

void ibl::ComputeBRDF(const ProgOptions& opts) {
    auto defines = std::vector<std::string>{};
    if (opts.multiScattering)
        defines.push_back("MULTISCATTERING");

    auto paths = std::vector{"glsl/brdf.vert"s, "glsl/brdf.frag"s, "glsl/common.frag"s};
    auto program = CompileAndLinkProgram("brdf", paths, defines);

    GLuint captureFBO, captureRBO;
    glCreateFramebuffers(1, &captureFBO);
    glCreateRenderbuffers(1, &captureRBO);

    glNamedRenderbufferStorage(captureRBO, GL_DEPTH_COMPONENT24, opts.texSize,
                               opts.texSize);
    glNamedFramebufferRenderbuffer(captureFBO, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                   captureRBO);

    GLuint brdfLUTTexture;
    glCreateTextures(GL_TEXTURE_2D, 1, &brdfLUTTexture);
    glTextureStorage2D(brdfLUTTexture, 1, GL_RG16F, opts.texSize, opts.texSize);
    glTextureParameteri(brdfLUTTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUTTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUTTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(brdfLUTTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glNamedFramebufferTexture(captureFBO, GL_COLOR_ATTACHMENT0, brdfLUTTexture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    glViewport(0, 0, opts.texSize, opts.texSize);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Quad quad;
    quad.setup();

    glUseProgram(program->id());
    glUniform1ui(1, opts.numSamples);
    quad.draw();

    std::size_t imageSize = 2 * opts.texSize * opts.texSize * sizeof(float);
    auto data = std::make_unique<std::byte[]>(imageSize);
    glGetTextureImage(brdfLUTTexture, 0, GL_RG, GL_FLOAT, imageSize, data.get());

    // Save Image
    util::SaveEXRImage(opts.outFile, opts.texSize, opts.texSize, 2,
                       reinterpret_cast<float*>(data.get()));
}