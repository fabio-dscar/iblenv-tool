#ifndef __IBL_SHADER_H__
#define __IBL_SHADER_H__

#include <iblenv.h>

namespace fs = std::filesystem;

namespace ibl {

enum class ShaderType : unsigned int;

class Shader {
public:
    Shader(const fs::path& path, ShaderType type, const std::string& src);

    void setVersion(const std::string& ver);
    void include(const std::string& source);

    unsigned int id() const { return handle; }
    void compile(const std::string& defines = "");

private:
    std::string getVersion();
    bool hasVersionDir();

    void handleIncludes();

    fs::path path;
    std::string name;
    std::string source;
    ShaderType type;
    unsigned int handle = 0;
};

class Program {
public:
    explicit Program(const std::string& name) : name(name) {}
    ~Program();

    void addShader(const Shader& src);
    unsigned int id() const { return handle; }
    void link();
    void cleanShaders();

private:
    std::vector<unsigned int> srcHandles;
    std::string name;
    unsigned int handle = 0;
};

Shader LoadShaderFile(const fs::path& filePath);
Shader LoadShaderFile(ShaderType type, const fs::path& filePath);

std::unique_ptr<Program> CompileAndLinkProgram(const std::string& name,
                                               std::span<std::string> sourceNames,
                                               std::span<std::string> definesList = {});

std::string BuildDefinesBlock(std::span<std::string> defines);
std::string GetShaderLog(unsigned int handle);
std::string GetProgramError(unsigned int handle);

} // namespace ibl

#endif // __IBL_SHADER_H__