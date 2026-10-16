#include "platform/pc/InputGameState.hpp"
#include "game/core/Minecraft.hpp"
#include "game/item/Inventory.hpp"
#include "game/world/generation/MultiplayerLocalPlayer.hpp"

int PcInputGetSelectedHotbarSlot(int player)
{
    if (player < 0 || player >= 4)
    {
        return -1;
    }
    Minecraft* minecraft = Minecraft::GetInstance();
    if (minecraft == NULL)
    {
        return -1;
    }
    boost::shared_ptr<MultiplayerLocalPlayer> localPlayer = minecraft->mLocalPlayers[player];
    if (localPlayer == NULL || localPlayer->mInventory == NULL)
    {
        return -1;
    }
    return localPlayer->mInventory->m_selected;
}
