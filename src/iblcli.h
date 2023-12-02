#ifndef __IBLCLI_H__
#define __IBLCLI_H__

#include <cstdlib>
#include <shader.h>
#include <iostream>
#include <util.h>
#include <format>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

using namespace std::literals;

namespace ibl {

unsigned int quadVAO, quadVBO;

GLFWwindow* window;
std::unique_ptr<Program> program;
void RenderQuad() {
    if (quadVAO == 0) {
        // Positions and uvs
        float quadVertices[] = {
            -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
        };

        std::size_t vertexSize = 5 * sizeof(float);

        glCreateVertexArrays(1, &quadVAO);
        glCreateBuffers(1, &quadVBO);
        glEnableVertexArrayAttrib(quadVAO, 0);
        glVertexArrayVertexBuffer(quadVAO, 0, quadVBO, 0, vertexSize);
        glVertexArrayAttribFormat(quadVAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glEnableVertexArrayAttrib(quadVAO, 1);
        glVertexArrayVertexBuffer(quadVAO, 1, quadVBO, 3 * sizeof(float), vertexSize);
        glVertexArrayAttribFormat(quadVAO, 1, 2, GL_FLOAT, GL_FALSE, 0);
        glNamedBufferStorage(quadVBO, sizeof(quadVertices), &quadVertices, 0);
    }

    glBindVertexArray(quadVAO);
    glUseProgram(program->id());
    glUniform1ui(1, 8192);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void InitOpenGL() {
    if (!glfwInit())
        util::ExitWithError("[ERROR] Couldn't initialize OpenGL context.\n");

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = glfwCreateWindow(640, 480, "Simple example", NULL, NULL);
    if (!window) {
        glfwTerminate();
        util::ExitWithError("[Error] Couldn't create GLFW window.\n");
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
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

void ComputeBRDF(unsigned int texSize, unsigned int samples) {
    auto paths = std::vector{"glsl/brdf.vert"s, "glsl/brdf.frag"s, "glsl/common.frag"s};
    program = CompileAndLinkProgram("brdf", paths);

    GLuint captureFBO, captureRBO;
    glCreateFramebuffers(1, &captureFBO);
    glCreateRenderbuffers(1, &captureRBO);

    glNamedRenderbufferStorage(captureRBO, GL_DEPTH_COMPONENT24, texSize, texSize);
    glNamedFramebufferRenderbuffer(captureFBO, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                   captureRBO);

    GLuint brdfLUTTexture;
    glCreateTextures(GL_TEXTURE_2D, 1, &brdfLUTTexture);
    glTextureStorage2D(brdfLUTTexture, 1, GL_RG16F, texSize, texSize);
    glTextureParameteri(brdfLUTTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUTTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTextureParameteri(brdfLUTTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(brdfLUTTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glNamedFramebufferTexture(captureFBO, GL_COLOR_ATTACHMENT0, brdfLUTTexture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

    glViewport(0, 0, texSize, texSize);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    RenderQuad();

    std::size_t imageSize = 2 * texSize * texSize * sizeof(float);
    auto data = std::make_unique<std::byte[]>(imageSize);
    glGetTextureImage(brdfLUTTexture, 0, GL_RG, GL_FLOAT, imageSize, data.get());

    // Save Image
    util::SaveEXRImage("brdf2.exr", texSize, texSize, 2,
                       reinterpret_cast<float*>(data.get()));

    glfwDestroyWindow(window);
    glfwTerminate();
}

} // namespace ibl

#endif // __IBLCLI_H__