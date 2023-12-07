#include <iblcli.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <shader.h>
#include <iostream>
#include <util.h>
#include <format>
#include <geometry.h>

#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/mat4x4.hpp>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <texture.h>
#include <framebuffer.h>

using namespace ibl;
using namespace std::literals;

GLFWwindow* window;

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
    glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

void ibl::Cleanup() {
    CleanupGeometry();

    if (window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

void ibl::ComputeBRDF(const CliOptions& opts) {
    auto defines = std::vector<std::string>{};
    if (opts.multiScattering)
        defines.emplace_back("MULTISCATTERING");
    if (opts.flipUv)
        defines.emplace_back("FLIP_V");

    auto shaders = std::vector{"brdf.vert"s, "brdf.frag"s};
    auto program = CompileAndLinkProgram("brdf", shaders, defines);

    GLenum intFormat = opts.useHalfFloat ? GL_RG16F : GL_RG32F;
    Texture brdfLUT{GL_TEXTURE_2D, intFormat, opts.texSize};

    Framebuffer fb{};
    fb.addDepthBuffer(brdfLUT.width, brdfLUT.height);
    fb.addTextureBuffer(GL_COLOR_ATTACHMENT0, brdfLUT);
    fb.bind();

    glViewport(0, 0, brdfLUT.width, brdfLUT.height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(program->id());
    glUniform1i(1, opts.numSamples);

    RenderQuad();

    ImageFormat fmt;
    fmt.width = brdfLUT.width;
    fmt.height = brdfLUT.height;
    fmt.numChannels = 2;
    fmt.compSize = opts.useHalfFloat ? 2 : 4;

    util::SaveImage(opts.outFile, {fmt, brdfLUT.data()});
}

glm::mat4 ScaleAndRotateY(const glm::vec3& scale, float degs) {
    auto I = glm::identity<glm::mat4>();
    return glm::scale(I, scale) * glm::rotate(I, glm::radians(degs), {0, 1, 0});
}

std::unique_ptr<Texture> ibl::SphericalProjToCubemap(const std::string& filePath,
                                                     int cubeSize, float degs,
                                                     bool swapHand) {

    auto shaders = std::vector{"convert.vert"s, "convert.frag"s};
    auto program = CompileAndLinkProgram("convert", shaders);

    auto img = util::LoadImage(filePath);
    Texture rectMap{GL_TEXTURE_2D, GL_RGB32F, img->width, img->height, {}};
    rectMap.uploadData(img->data.get());

    Framebuffer fb{};
    fb.addDepthBuffer(cubeSize, cubeSize);
    fb.bind();

    auto cubemap = std::make_unique<Texture>(
        GL_TEXTURE_CUBE_MAP, GL_RGB32F, cubeSize, cubeSize,
        SamplerOpts{.minFilter = GL_LINEAR_MIPMAP_LINEAR}, 0, MaxMipLevel(cubeSize));

    auto projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 5.0f);
    auto modelMatrix = ScaleAndRotateY({1, 1, swapHand ? 1 : -1}, degs);

    glViewport(0, 0, cubeSize, cubeSize);
    glUseProgram(program->id());
    glUniformMatrix4fv(Projection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(Model, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniform1i(EnvMap, 0);
    glActiveTexture(GL_TEXTURE0);
    rectMap.bind();
    glCullFace(swapHand ? GL_FRONT : GL_BACK);
    for (int face = 0; face < 6; ++face) {
        glUniformMatrix4fv(View, 1, GL_FALSE, glm::value_ptr(CubeMapViews[face]));
        fb.addTextureLayer(GL_COLOR_ATTACHMENT0, *cubemap, face);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        RenderCube();
    }
    glCullFace(swapHand ? GL_BACK : GL_FRONT);

    return cubemap;
}

void ibl::ConvertToCubemap(const CliOptions& opts) {
    auto cubemap = SphericalProjToCubemap(opts.inFile, opts.texSize);
    util::ExportCubemap(*cubemap);
}

void ibl::ComputeIrradiance(const CliOptions& opts) {
    auto defines = std::vector<std::string>{};
    if (opts.divideLambertConstant)
        defines.emplace_back("DIVIDED_PI");
    if (opts.usePrefilteredIS)
        defines.emplace_back("PREFILTERED_IS");

    auto shaders = std::vector{"convert.vert"s, "irradiance.frag"s};
    auto program = CompileAndLinkProgram("irradiance", shaders, defines);

    // Convert first if not in cubemap format yet
    std::unique_ptr<Texture> envMap = nullptr;
    if (opts.isInputEquirect) {
        envMap = SphericalProjToCubemap(opts.inFile, opts.texSize);
    } else {
        // envMap = LoadCubemap(opts.inFile);  Load directly
    }
    envMap->generateMipmaps();

    Framebuffer fb{};
    fb.addDepthBuffer(opts.texSize, opts.texSize);
    fb.bind();

    Texture irradiance{GL_TEXTURE_CUBE_MAP, GL_RGB32F, opts.texSize, opts.texSize, {}, 6};

    auto projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 5.0f);
    auto modelMatrix = ScaleAndRotateY({1, 1, 1}, 0);

    glUseProgram(program->id());
    glUniform1i(EnvMap, 0);
    glUniform1i(NumSamples, opts.numSamples);
    glUniformMatrix4fv(Projection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(Model, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glActiveTexture(GL_TEXTURE0);
    envMap->bind();
    glViewport(0, 0, opts.texSize, opts.texSize);
    glDisable(GL_CULL_FACE);
    for (int face = 0; face < 6; ++face) {
        glUniformMatrix4fv(View, 1, GL_FALSE, glm::value_ptr(CubeMapViews[face]));
        fb.addTextureLayer(GL_COLOR_ATTACHMENT0, irradiance, face);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        RenderCube();
    }
    glEnable(GL_CULL_FACE);

    util::ExportCubemap(irradiance);
}

void ibl::ComputeConvolution(const CliOptions& opts) {
    auto defines = std::vector<std::string>{};
    if (opts.usePrefilteredIS)
        defines.emplace_back("PREFILTERED_IS");

    auto shaders = std::vector{"convert.vert"s, "convolution.frag"s};
    auto program = CompileAndLinkProgram("convolution", shaders, defines);

    // Convert first if not in cubemap format yet
    std::unique_ptr<Texture> envMap = nullptr;
    if (opts.isInputEquirect) {
        envMap = SphericalProjToCubemap(opts.inFile, opts.texSize);
    } else {
        // envMap = LoadCubemap(opts.inFile);  Load directly
    }
    envMap->generateMipmaps();

    Framebuffer fb{};
    fb.addDepthBuffer(opts.texSize, opts.texSize);
    fb.bind();

    Texture convMap{GL_TEXTURE_CUBE_MAP, GL_RGB32F, opts.texSize, opts.texSize, {}, 6,
                    opts.mipLevels};
    convMap.generateMipmaps();

    auto projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 5.0f);
    auto modelMatrix = ScaleAndRotateY({1, 1, -1}, 0);

    glUseProgram(program->id());
    glUniform1i(EnvMap, 0);
    glUniform1i(NumSamples, opts.numSamples);
    glUniformMatrix4fv(Projection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(Model, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glActiveTexture(GL_TEXTURE0);
    envMap->bind();
    glDisable(GL_CULL_FACE);
    for (int mip = 0; mip < opts.mipLevels; ++mip) {
        int mipSize = opts.texSize * std::pow(0.5, mip);
        glViewport(0, 0, mipSize, mipSize);
        fb.resize(mipSize, mipSize);

        float rough = (float)mip / (float)(opts.mipLevels - 1);
        glUniform1f(Roughness, rough);

        for (int face = 0; face < 6; ++face) {
            glUniformMatrix4fv(View, 1, GL_FALSE, glm::value_ptr(CubeMapViews[face]));
            fb.addTextureLayer(GL_COLOR_ATTACHMENT0, convMap, face, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            RenderCube();
        }
    }
    glEnable(GL_CULL_FACE);

    util::ExportCubemap(convMap);
}