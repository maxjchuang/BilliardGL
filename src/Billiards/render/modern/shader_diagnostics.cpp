#include "render/modern/shader_diagnostics.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace billiardgl::render {

const char* shaderStageName(ShaderStage stage) noexcept
{
    switch (stage) {
        case ShaderStage::Vertex: return "vertex";
        case ShaderStage::Fragment: return "fragment";
    }
    return "unknown";
}

namespace {

std::string numberedSource(const std::string& source)
{
    std::istringstream input(source);
    std::ostringstream output;
    std::string line;
    std::size_t lineNumber = 1;
    while (std::getline(input, line)) {
        output << lineNumber++ << " | " << line << '\n';
    }
    if (source.empty()) output << "(empty source)\n";
    return output.str();
}

std::string logOrPlaceholder(const std::string& driverLog)
{
    return driverLog.empty() ? "(driver returned no log)" : driverLog;
}

}  // namespace

std::string formatShaderCompilationError(ShaderStage stage,
                                         const std::string& resourceName,
                                         const std::string& driverLog,
                                         const std::string& source)
{
    std::ostringstream message;
    message << "Shader compilation failed\n"
            << "stage: " << shaderStageName(stage) << '\n'
            << "resource: " << resourceName << '\n'
            << "driver log:\n" << logOrPlaceholder(driverLog) << '\n'
            << "numbered source:\n" << numberedSource(source);
    return message.str();
}

std::string formatProgramLinkError(const std::string& resourceName,
                                   const std::string& driverLog)
{
    std::ostringstream message;
    message << "Shader program link failed\n"
            << "resource: " << resourceName << '\n'
            << "driver log:\n" << logOrPlaceholder(driverLog) << '\n';
    return message.str();
}

std::string formatShaderFileError(const std::string& path,
                                  const std::string& detail)
{
    return "Shader source file error\npath: " + path + "\ndetail: " + detail + '\n';
}

std::string readShaderSourceFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(formatShaderFileError(path, "could not open for reading"));
    }
    std::ostringstream source;
    source << input.rdbuf();
    if (!input.good() && !input.eof()) {
        throw std::runtime_error(formatShaderFileError(path, "read failed"));
    }
    return source.str();
}

}  // namespace billiardgl::render
