#include "game/command/chat/ClearChatCommand.hpp"

#include "game/command/CommandArguments.hpp"
#include "game/item/Inventory.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/ui/AbstractContainerMenu.hpp"

using CommandArguments::ParseInt;

ClearChatCommand::ClearChatCommand()
    : ChatCommand(L"clear", L"[player] [item] [data] [maxCount]", L"targets item number number", ePermission_Operator)
{
}

bool ClearChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() > 5)
        return false;
    Item* item = NULL;
    int data = -1;
    int maximum = -1;
    if (words.size() > 3 && !ParseInt(words[3], -1, 32767, &data))
    {
        Fail(source, L"The data value must be a whole number from -1 to 32767.");
        return true;
    }
    if (words.size() > 4 && !ParseInt(words[4], -1, 2147483647, &maximum))
    {
        Fail(source, L"The maximum count must be -1 or more.");
        return true;
    }
    PlayerVector targets;
    if (!ResolvePlayers(source, words, 1, targets))
        return true;
    if (words.size() > 2)
    {
        item = FindItem(source, words[2]);
        if (item == NULL)
            return true;
    }
    for (size_t index = 0; index < targets.size(); ++index)
    {
        const PlayerPtr& target = targets[index];
        const int removed = target->mInventory->clearInventory(item, data, maximum, NULL);
        target->mInventoryMenu->broadcastChanges();
        if (maximum == 0)
            Tell(source,
                 target->getName() + L" has " + Count(removed, L"item", L"items") + L" that match the criteria");
        else if (removed > 0)
            Tell(source,
                 L"Cleared the inventory of " + target->getName() + L", removing " + Count(removed, L"item", L"items"));
        else
            Fail(source, L"Could not clear the inventory of " + target->getName() + L", no items to remove");
    }
    return true;
}
