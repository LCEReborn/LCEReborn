#include "platform/pc/PcLaunchOptions.hpp"
#include "platform/pc/PcServer.hpp"

int main(int argc, char** argv)
{
    int exitCode;
    if (!PcParseLaunchOptions(argc, argv, exitCode, PcProgram::Server))
        return exitCode;
    return PcServerMain();
}
