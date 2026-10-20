// Host tool: emit the shared shader sources for offline SPIR-V compilation.
#include "platform/pc/RenderShaders.hpp"
#include <fstream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 2)
        return 1;
    for (int stage = 0; stage < 2; ++stage)
    {
        const bool vertex = stage == 0;
        const int count = vertex ? RenderVertexShader_Count : RenderPixelShader_Count;
        for (int index = 0; index < count; ++index)
        {
            std::string source = vertex ? RenderVertexShaderSource(index) : RenderPixelShaderSource(index);
            std::ofstream file(std::string(argv[1]) + "/" + (vertex ? "v" : "p") + std::to_string(index)
                               + (vertex ? ".vert" : ".frag"));
            file << source;
            if (!file)
                return 1;
        }
    }
    return 0;
}
