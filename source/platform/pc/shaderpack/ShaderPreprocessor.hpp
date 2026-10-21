#ifndef PLATFORM_PC_SHADERPACK_SHADERPREPROCESSOR_HPP
#define PLATFORM_PC_SHADERPACK_SHADERPREPROCESSOR_HPP

#include <map>
#include <string>

typedef std::map<std::string, std::string> ShaderMacros;

std::string ShaderPreprocessConditionals(const std::string& text, ShaderMacros macros, bool keepDefines = false);
double ShaderEvaluateExpression(const std::string& expression, const ShaderMacros& macros);

#endif
