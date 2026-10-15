#include "game/network/CPlatformNetworkManager.hpp"
#include "platform/Platform.hpp"
#if LCE_PLATFORM_WIIU
#include "platform/wiiu/CPlatformNetworkManagerNintendo.hpp"
#elif LCE_PLATFORM_PC
#include "platform/pc/CPlatformNetworkManagerPC.hpp"
#endif

CPlatformNetworkManager* CPlatformNetworkManager::Create()
{
#if LCE_PLATFORM_WIIU
    return new CPlatformNetworkManagerNintendo;
#elif LCE_PLATFORM_PC
    return new CPlatformNetworkManagerPC;
#endif
}

void CPlatformNetworkManager::CancelJoinGame()
{
}

void CPlatformNetworkManager::FakeLocalPlayerJoined()
{
}
