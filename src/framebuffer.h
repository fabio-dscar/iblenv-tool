#ifndef __FRAMEBUFFER_H__
#define __FRAMEBUFFER_H__

#include <glad/glad.h>
#include <texture.h>

namespace ibl {

class Framebuffer {
public:
    Framebuffer() { glCreateFramebuffers(1, &handle); }
    ~Framebuffer() {
        if (depthBuff)
            glDeleteRenderbuffers(1, &depthBuff);

        if (handle != 0)
            glDeleteFramebuffers(1, &handle);
    }

    void addDepthBuffer(unsigned int width, unsigned int height) {
        glCreateRenderbuffers(1, &depthBuff);

        glNamedRenderbufferStorage(depthBuff, GL_DEPTH_COMPONENT24, width, height);
        glNamedFramebufferRenderbuffer(handle, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                       depthBuff);
    }

    void addTextureBuffer(GLenum attachment, const Texture& tex) {
        glNamedFramebufferTexture(handle, attachment, tex.handle, 0);
    }

    void addTextureLayer(GLenum attachment, const Texture& tex, int layer) {
        glNamedFramebufferTextureLayer(handle, attachment, tex.handle, 0, layer);
    }

    void bind() { glBindFramebuffer(GL_FRAMEBUFFER, handle); }

    GLuint handle;
    GLuint depthBuff;
};

} // namespace ibl

#endif // __FRAMEBUFFER_H__