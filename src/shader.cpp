#include "glad/glad.h"
#include <filesystem>
#include <shader.h>
#include <util.h>
#include <format>
#include <iostream>

using namespace ibl;
using namespace std::filesystem;

Shader::~Shader() {
    // if (handle != 0)
    //     glDeleteShader(handle);
}

bool Shader::compile() {
    handle = glCreateShader(type);
    if (handle == 0)
        util::ExitWithError(std::format("Could not create shader {}", name));

    const char* cstr = source.c_str();
    glShaderSource(handle, 1, &cstr, 0);
    glCompileShader(handle);

    GLint result;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &result);
    if (result == GL_TRUE)
        return true;

    std::string message = GetShaderLog(handle);
    util::ExitWithError(std::format("Shader {} compilation log:\n{}", name, message));

    return false;
}

Program::~Program() {
    if (handle != 0)
        glDeleteProgram(handle);
}

void Program::addShader(const Shader& src) {
    srcHandles.push_back(src.id());
}

bool Program::link() {
    handle = glCreateProgram();
    if (handle == 0) {
        std::string message = GetProgramError(handle);
        util::ExitWithError("Could not create program " + name);
    }

    for (GLuint sid : srcHandles)
        glAttachShader(handle, sid);

    glLinkProgram(handle);

    GLint res;
    glGetProgramiv(handle, GL_LINK_STATUS, &res);
    if (res != GL_TRUE) {
        std::string message = GetProgramError(handle);
        util::ExitWithError(message);

        for (GLuint sid : srcHandles)
            glDetachShader(handle, sid);

        glDeleteProgram(handle);

        return false;
    }

    // Detach shaders after successful linking
    for (GLuint sid : srcHandles)
        glDetachShader(handle, sid);

    return true;
}

void Program::cleanShaders() {
    for (auto sid : srcHandles)
        glDeleteShader(sid);
}

std::string ibl::GetShaderLog(unsigned int handle) {
    GLint logLen;
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);

    auto log = std::make_unique<char[]>(logLen);
    glGetShaderInfoLog(handle, logLen, &logLen, log.get());

    return {log.get()};
}

std::string ibl::GetProgramError(unsigned int handle) {
    GLint logLen;
    glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &logLen);

    auto log = std::make_unique<char[]>(logLen);
    glGetProgramInfoLog(handle, logLen, &logLen, log.get());

    return {log.get()};
}

Shader ibl::LoadShaderFile(const std::string& filePath) {
    ShaderType type = FRAGMENT_SHADER;

    // Deduce type from extension, if possible
    auto ext = path(filePath).extension();
    if (ext == ".frag" || ext == ".fs")
        type = FRAGMENT_SHADER;
    else if (ext == ".vert" || ext == ".vs")
        type = VERTEX_SHADER;
    else
        util::ExitWithError(std::format("Couldn't deduce type for shader: {}", filePath));

    return LoadShaderFile(type, filePath);
}

Shader ibl::LoadShaderFile(ShaderType type, const std::string& filePath) {
    auto source = util::ReadFile(filePath, std::ios_base::in);
    if (!source)
        util::ExitWithError(
            std::format("[ERROR] Couldn't load shader file {}", filePath));

    return {path(filePath).filename(), type, source.value()};
}

std::unique_ptr<Program>
ibl::CompileAndLinkProgram(const std::string& name,
                        std::span<const std::string> sourcePaths) {
    auto program = std::make_unique<Program>(name);
    for (auto& path : sourcePaths) {
        Shader s = LoadShaderFile(path);
        s.compile();
        program->addShader(s);
    }
    program->link();
    program->cleanShaders();
    return program;
}