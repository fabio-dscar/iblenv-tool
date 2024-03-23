#include <shader.h>

#include <glad/glad.h>

#include <regex>
#include <iostream>

#include <util.h>

using namespace ibl;
using namespace ibl::util;

namespace {

const std::string DefaultVer = "460 core";
const fs::path ShaderFolder = "./glsl";

} // namespace

enum class ibl::ShaderType : unsigned int {
    Vertex = GL_VERTEX_SHADER,
    Fragment = GL_FRAGMENT_SHADER,
    Geometry = GL_GEOMETRY_SHADER,
    Compute = GL_COMPUTE_SHADER
};

Shader::Shader(const fs::path& path, ShaderType type, const std::string& src)
    : path(path), name(path.filename().string()), source(src), type(type) {
    handle = glCreateShader(static_cast<GLenum>(type));
    if (handle == 0)
        FATAL("Could not create shader {}", name);

    if (!hasVersionDir())
        setVersion(DefaultVer);

    handleIncludes();
}

void Shader::handleIncludes() {
    std::regex rgx(R"([ ]*#[ ]*include[ ]+[\"<](.*)[\">].*)");
    std::smatch smatch;
    std::vector processed{name};

    while (std::regex_search(source, smatch, rgx)) {
        auto file = smatch[1].str();
        if (std::find(processed.begin(), processed.end(), file) != processed.end())
            FATAL("Repeated/Recursively including '{}' at '{}'.", file, name);

        auto filePath = path.parent_path() / file;
        auto src = util::ReadTextFile(filePath);
        if (!src)
            FATAL("Couldn't open included shader '{}' in '{}'", file, name);

        source.replace(smatch.position(), smatch.length(), src.value());

        processed.push_back(file);
    }
}

bool Shader::hasVersionDir() {
    return source.find("#version ") != std::string::npos;
}

void Shader::setVersion(const std::string& ver) {
    auto start = source.find("#version");

    auto directive = std::format("#version {}\n", ver);
    if (start != std::string::npos) {
        auto end = source.find('\n', start);
        source.replace(start, end - start, directive);
    } else {
        source.insert(0, directive);
    }
}

void Shader::include(const std::string& include) {
    auto start = source.find("#version");
    auto end = 0;

    if (start != std::string::npos)
        end = source.find('\n', start);

    source.insert(end + 1, include);
}

void Shader::compile(const std::string& defines) {
    if (handle == 0)
        FATAL("Trying to compile shader {} with invalid handle.", name);

    include(defines);

    const char* sources[] = {source.c_str()};
    glShaderSource(handle, 1, sources, 0);
    glCompileShader(handle);

    GLint result;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &result);
    if (result != GL_TRUE) {
        std::string message = GetShaderLog(handle);
        FATAL("Shader {} compilation log:\n{}", name, message);
    }
}

Program::~Program() {
    if (handle != 0)
        glDeleteProgram(handle);
}

void Program::addShader(const Shader& src) {
    srcHandles.push_back(src.id());
}

void Program::link() {
    handle = glCreateProgram();
    if (handle == 0) {
        std::string message = GetProgramError(handle);
        FATAL("Could not create program {}", name);
    }

    for (GLuint sid : srcHandles)
        glAttachShader(handle, sid);

    glLinkProgram(handle);

    for (GLuint sid : srcHandles)
        glDetachShader(handle, sid);

    GLint res;
    glGetProgramiv(handle, GL_LINK_STATUS, &res);
    if (res != GL_TRUE) {
        std::string message = GetProgramError(handle);
        FATAL("Program linking error: {}", message);
    }
}

void Program::cleanShaders() {
    for (auto sid : srcHandles)
        if (glIsShader(sid) == GL_TRUE)
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

Shader ibl::LoadShaderFile(const fs::path& filePath) {
    using enum ShaderType;
    ShaderType type = Fragment;

    // Deduce type from extension, if possible
    auto ext = filePath.extension();
    if (ext == ".frag" || ext == ".fs")
        type = Fragment;
    else if (ext == ".vert" || ext == ".vs")
        type = Vertex;
    else
        FATAL("Couldn't deduce type for shader: {}", filePath.string());

    return LoadShaderFile(type, filePath);
}

Shader ibl::LoadShaderFile(ShaderType type, const fs::path& filePath) {
    auto source = util::ReadTextFile(filePath);
    if (!source.has_value())
        FATAL("Couldn't load shader file {}", filePath.string());

    return {filePath, type, source.value()};
}

std::string ibl::BuildDefinesBlock(std::span<std::string> defines) {
    std::string defBlock = "";
    for (auto& def : defines) {
        if (!def.empty())
            defBlock.append(std::format("#define {}\n", def));
    }
    return defBlock;
}

std::unique_ptr<Program> ibl::CompileAndLinkProgram(const std::string& name,
                                                    std::span<std::string> sourceNames,
                                                    std::span<std::string> definesList) {

    auto program = std::make_unique<Program>(name);
    auto defines = BuildDefinesBlock(definesList);

    for (auto& fname : sourceNames) {
        Shader s = LoadShaderFile(ShaderFolder / fname);
        s.compile(defines);
        program->addShader(s);
    }

    program->link();
    program->cleanShaders();
    return program;
}