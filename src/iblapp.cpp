#include <iblapp.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <shader.h>
#include <util.h>
#include <geometry.h>
#include <texture.h>
#include <framebuffer.h>
#include <cubemap.h>

#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <glm/mat4x4.hpp>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace ibl;
using namespace ibl::util;
using namespace std::literals;

static GLFWwindow* window;

void ibl::InitOpenGL() {
    if (!glfwInit())
        FATAL("Couldn't initialize OpenGL context.");

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = glfwCreateWindow(640, 480, "Simple example", NULL, NULL);
    if (!window) {
        glfwTerminate();
        FATAL("Couldn't create GLFW window.");
    }
    glfwMakeContextCurrent(window);

    int glver = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    if (glver == 0)
        FATAL("Failed to initialize OpenGL context");

#ifdef DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(OpenGLErrorCallback, 0);
#endif

    // Print system info
    auto renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    auto vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    auto version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    auto glslVer =
        reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    Print("OpenGL Renderer: {} ({})", renderer, vendor);
    Print("OpenGL Version: {}", version);
    Print("GLSL Version: {}\n", glslVer);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDepthRange(0.0, 1.0);
    glClearDepth(1.0);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    glDisable(GL_CULL_FACE); // We're rendering skybox back faces
}

void ibl::Cleanup() {
    CleanupGeometry();

    if (window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

void ibl::ComputeBRDF(const CliOptions& opts) {
    Print("Computing BRDF to {} 2-channel {}x{} float texture at {} spp",
          opts.useHalf ? "16 bit" : "32 bit", opts.texSize, opts.texSize,
          opts.numSamples);

    auto defines = GetShaderDefines(opts);
    auto shaders = std::array{"brdf.vert"s, "brdf.frag"s};
    auto program = CompileAndLinkProgram("brdf", shaders, defines);

    GLenum intFormat = opts.useHalf ? GL_RG16F : GL_RG32F;
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

    SaveImage(opts.outFile, *brdfLUT.image());
}

glm::mat4 ScaleAndRotateY(const glm::vec3& scale, float degs) {
    auto I = glm::identity<glm::mat4>();
    return glm::scale(I, scale) * glm::rotate(I, glm::radians(degs), {0, 1, 0});
}

std::unique_ptr<Texture> ibl::SphericalProjToCubemap(const std::string& filePath,
                                                     int cubeSize, float degs,
                                                     bool swapHand) {
    Print("Converting spherical projection [to {}px cube]", cubeSize);

    auto shaders = std::array{"convert.vert"s, "convert.frag"s};
    auto program = CompileAndLinkProgram("convert", shaders);

    auto img = util::LoadImage(filePath);
    auto imgFmt = img->format();
    if ((imgFmt.width / 2) != imgFmt.height)
        FATAL("Input is not an equirectangular mapping.");

    Texture rectMap{GL_TEXTURE_2D, GL_RGB32F, imgFmt.width, imgFmt.height, 1};
    rectMap.upload(*img);

    Framebuffer fb{};
    fb.addDepthBuffer(cubeSize, cubeSize);
    fb.bind();

    auto cubemap = std::make_unique<Texture>(GL_TEXTURE_CUBE_MAP, GL_RGB32F, cubeSize,
                                             MaxMipLevel(cubeSize));
    cubemap->setParam(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    auto projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 5.0f);
    auto modelMatrix = ScaleAndRotateY({1, 1, swapHand ? -1 : 1}, degs);

    glUseProgram(program->id());
    glUniformMatrix4fv(Projection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(Model, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniform1i(EnvMap, 0);

    glActiveTexture(GL_TEXTURE0);
    rectMap.bind();

    glViewport(0, 0, cubeSize, cubeSize);
    for (int face = 0; face < 6; ++face) {
        glUniformMatrix4fv(View, 1, GL_FALSE, glm::value_ptr(CubeMapViews[face]));
        fb.addTextureLayer(GL_COLOR_ATTACHMENT0, *cubemap, face);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        RenderCube();
    }

    return cubemap;
}

auto LoadEnvironment(const CliOptions& opts, ImageFormat* reqFmt = nullptr) {
    if (opts.isInputEquirect)
        return SphericalProjToCubemap(opts.inFile, opts.texSize);

    auto cube = ImportCubeMap(opts.inFile, opts.importType, reqFmt);
    return std::make_unique<Texture>(*cube);
}

void ibl::ConvertToCubemap(const CliOptions& opts) {
    std::unique_ptr<CubeImage> cube;
    if (opts.isInputEquirect) {
        // Convert to cubemap and then retrieve data from gpu
        auto cubeTex = SphericalProjToCubemap(opts.inFile, opts.texSize);
        cubeTex->levels = 1; // Only 1 level
        cube = cubeTex->cubemap();
    } else {
        cube = ImportCubeMap(opts.inFile, opts.importType, nullptr);
    }

    Print("Converting cubemap to '{}'", LayoutNames.at(opts.exportType));

    ExportCubemap(opts.outFile, opts.exportType, *cube);
}

void ibl::ComputeIrradiance(const CliOptions& opts) {
    auto defines = GetShaderDefines(opts);
    auto shaders = std::array{"convert.vert"s, "irradiance.frag"s};
    auto program = CompileAndLinkProgram("irradiance", shaders, defines);

    auto envMap = LoadEnvironment(opts);
    envMap->generateMipmaps();
    envMap->setParam(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    Framebuffer fb{};
    fb.addDepthBuffer(opts.texSize, opts.texSize);
    fb.bind();

    GLuint intFormat = opts.useHalf ? GL_RGB16F : GL_RGB32F;
    Texture irradiance{GL_TEXTURE_CUBE_MAP, intFormat, opts.texSize};

    auto projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 5.0f);
    auto modelMatrix = ScaleAndRotateY({1, 1, 1}, 0);

    Print("Computing irradiance [{}px cube, {} spp, {} prefiltered IS]", opts.texSize,
          opts.numSamples, opts.usePrefilteredIS ? "with" : "without");

    glUseProgram(program->id());
    glUniform1i(EnvMap, 0);
    glUniform1i(NumSamples, opts.numSamples);
    glUniformMatrix4fv(Projection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(Model, 1, GL_FALSE, glm::value_ptr(modelMatrix));

    glActiveTexture(GL_TEXTURE0);
    envMap->bind();

    glViewport(0, 0, opts.texSize, opts.texSize);
    for (int face = 0; face < 6; ++face) {
        glUniformMatrix4fv(View, 1, GL_FALSE, glm::value_ptr(CubeMapViews[face]));
        fb.addTextureLayer(GL_COLOR_ATTACHMENT0, irradiance, face);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        RenderCube();
    }

    ExportCubemap(opts.outFile, opts.exportType, *irradiance.cubemap());
}

void ibl::ComputeSpecular(const CliOptions& opts) {
    auto defines = GetShaderDefines(opts);
    auto shaders = std::array{"convert.vert"s, "specular.frag"s};
    auto program = CompileAndLinkProgram("specular", shaders, defines);

    auto envMap = LoadEnvironment(opts);
    envMap->setParam(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    envMap->generateMipmaps();

    Framebuffer fb{};
    fb.addDepthBuffer(opts.texSize, opts.texSize);
    fb.bind();

    GLuint intFormat = opts.useHalf ? GL_RGB16F : GL_RGB32F;
    Texture convMap{GL_TEXTURE_CUBE_MAP, intFormat, opts.texSize, opts.mipLevels};
    convMap.generateMipmaps();

    auto projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 5.0f);
    auto modelMatrix = ScaleAndRotateY({1, 1, 1}, 0);

    Print("Computing cube specular convolution [{}px cube, {} levels, {} spp, {} prefiltered IS]",
          opts.texSize, opts.mipLevels, opts.numSamples,
          opts.usePrefilteredIS ? "with" : "without");

    glUseProgram(program->id());
    glUniform1i(EnvMap, 0);
    glUniform1i(NumSamples, opts.numSamples);
    glUniformMatrix4fv(Projection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(Model, 1, GL_FALSE, glm::value_ptr(modelMatrix));

    glActiveTexture(GL_TEXTURE0);
    envMap->bind();

    for (int mip = 0; mip < opts.mipLevels; ++mip) {
        int mipSize = opts.texSize * std::pow(0.5, mip);
        glViewport(0, 0, mipSize, mipSize);
        fb.resize(mipSize, mipSize);

        float rough = mip / (opts.mipLevels - 1.0f);
        glUniform1f(Roughness, rough);

        for (int face = 0; face < 6; ++face) {
            glUniformMatrix4fv(View, 1, GL_FALSE, glm::value_ptr(CubeMapViews[face]));
            fb.addTextureLayer(GL_COLOR_ATTACHMENT0, convMap, face, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            RenderCube();
        }
    }

    ExportCubemap(opts.outFile, opts.exportType, *convMap.cubemap());
}

std::vector<std::string> ibl::GetShaderDefines(const CliOptions& opts) {
    auto defines = std::vector<std::string>{};

    using enum Mode;
    switch (opts.mode) {
    case Brdf:
        if (opts.multiScattering)
            defines.emplace_back("MULTISCATTERING");

        if (opts.flipUv)
            defines.emplace_back("FLIP_V");
        break;

    case Irradiance:
        if (opts.divideLambertConstant)
            defines.emplace_back("DIVIDED_PI");
        [[fallthrough]];
    case Specular:
        if (opts.usePrefilteredIS)
            defines.emplace_back("PREFILTERED_IS");
        break;

    default:
        break;
    }

    return defines;
}