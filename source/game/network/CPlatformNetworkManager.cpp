#include "game/network/CPlatformNetworkManager.hpp"
#include "net/CPlatformNetworkManagerLan.hpp"

CPlatformNetworkManager* CPlatformNetworkManager::Create()
{
    return new CPlatformNetworkManagerLan;
}

void CPlatformNetworkManager::CancelJoinGame()
{
}

void CPlatformNetworkManager::FakeLocalPlayerJoined()
{
}
