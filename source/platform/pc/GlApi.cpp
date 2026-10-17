#include "platform/pc/GlApi.hpp"

#include "platform/pc/Log.hpp"

static const Logger sLog("OpenGL");

#include <stdio.h>

namespace Gl
{
#define LCE_GL_DEFINE(returnType, name, parameters) name##Function name = NULL;
LCE_GL_FUNCTIONS(LCE_GL_DEFINE)
#undef LCE_GL_DEFINE

bool LoadFunctions(void* (*getProcAddress)(const char* name))
{
    bool bComplete = true;

#define LCE_GL_LOAD(returnType, name, parameters)                                                                      \
    name = reinterpret_cast<name##Function>(getProcAddress("gl" #name));                                               \
    if (name == NULL)                                                                                                  \
    {                                                                                                                  \
        LOG_ERROR(sLog, "OpenGL function gl%s is unavailable", #name);                                                 \
        bComplete = false;                                                                                             \
    }
    LCE_GL_FUNCTIONS(LCE_GL_LOAD)
#undef LCE_GL_LOAD

    return bComplete;
}
}
