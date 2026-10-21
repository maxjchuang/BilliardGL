#pragma once

#include <string>

namespace billiardgl::render {

enum class ShaderStage {
    Vertex,
    Fragment,
};

const char* shaderStageName(ShaderStage stage) noexcept;

std::string formatShaderCompilationError(ShaderStage stage,
                                         const std::string& resourceName,
                                         const std::string& driverLog,
                                         const std::string& source);

std::string formatProgramLinkError(const std::string& resourceName,
                                   const std::string& driverLog);

std::string formatShaderFileError(const std::string& path,
                                  const std::string& detail);

std::string readShaderSourceFile(const std::string& path);

}  // namespace billiardgl::render
