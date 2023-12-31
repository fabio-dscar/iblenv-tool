#include <geometry.h>

#include <glad/glad.h>
#include <vector>

using namespace ibl;

namespace {
GLuint QuadVao, QuadVbo;
GLuint CubeVao, CubeVbo, CubeVboIdx;
} // namespace

void ibl::RenderQuad() {
    if (QuadVao == 0) {
        // Positions and uvs
        const float quadVertices[] = {
            -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
            1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 1.0f,  -1.0f, 0.0f, 1.0f, 0.0f,
        };

        constexpr std::size_t vertexSize = 5 * sizeof(float);

        glCreateVertexArrays(1, &QuadVao);
        glCreateBuffers(1, &QuadVbo);
        glVertexArrayVertexBuffer(QuadVao, 0, QuadVbo, 0, vertexSize);
        glEnableVertexArrayAttrib(QuadVao, 0);
        glVertexArrayAttribBinding(QuadVao, 0, 0);
        glVertexArrayAttribFormat(QuadVao, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glEnableVertexArrayAttrib(QuadVao, 1);
        glVertexArrayAttribBinding(QuadVao, 1, 0);
        glVertexArrayAttribFormat(QuadVao, 1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float));
        glNamedBufferStorage(QuadVbo, sizeof(quadVertices), &quadVertices, 0);
    }

    glBindVertexArray(QuadVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void ibl::RenderCube() {
    if (CubeVao == 0) {
        float vertices[] = {-1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f, -1.0f,
                            -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
                            -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f, 1.0f};

        unsigned int indices[] = {0, 1, 2, 1, 0, 3, 4, 5, 7, 7, 6, 4, 6, 3, 0, 0, 4, 6,
                                  7, 2, 1, 2, 7, 5, 0, 2, 5, 5, 4, 0, 3, 7, 1, 7, 3, 6};

        constexpr std::size_t vertexSize = 3 * sizeof(float);

        glCreateVertexArrays(1, &CubeVao);
        glCreateBuffers(1, &CubeVbo);
        glVertexArrayVertexBuffer(CubeVao, 0, CubeVbo, 0, vertexSize);
        glEnableVertexArrayAttrib(CubeVao, 0);
        glVertexArrayAttribBinding(QuadVao, 0, 0);
        glVertexArrayAttribFormat(CubeVao, 0, 3, GL_FLOAT, GL_FALSE, 0);
        glNamedBufferStorage(CubeVbo, sizeof(vertices), &vertices, 0);
        glCreateBuffers(1, &CubeVboIdx);
        glNamedBufferStorage(CubeVboIdx, sizeof(indices), &indices, 0);
        glVertexArrayElementBuffer(CubeVao, CubeVboIdx);
    }

    glBindVertexArray(CubeVao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
}

void ibl::CleanupGeometry() {
    if (QuadVao != 0) {
        glDeleteVertexArrays(1, &QuadVao);
        glDeleteBuffers(1, &QuadVbo);
    }

    if (CubeVao != 0) {
        glDeleteVertexArrays(1, &CubeVao);
        glDeleteBuffers(1, &CubeVbo);
        glDeleteBuffers(1, &CubeVboIdx);
    }
}
