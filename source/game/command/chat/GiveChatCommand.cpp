#include "game/command/chat/GiveChatCommand.hpp"

#include <algorithm>

#include "game/client/audio/SoundEvents.hpp"
#include "game/client/audio/SoundSource.hpp"
#include "game/command/CommandArguments.hpp"
#include "game/entity/mob/ItemEntity.hpp"
#include "game/item/Inventory.hpp"
#include "game/item/Item.hpp"
#include "game/item/ItemInstance.hpp"
#include "game/item/not_null_ptr.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/ui/AbstractContainerMenu.hpp"
#include "game/util/Random.hpp"
#include "game/world/Level.hpp"

using CommandArguments::Number;
using CommandArguments::ParseInt;

GiveChatCommand::GiveChatCommand()
    : ChatCommand(L"give", L"<player> <item> [amount] [data]", L"targets item number number", ePermission_Operator)
{
}

bool GiveChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() < 3 || words.size() > 5)
        return false;
    int amount = 1;
    int data = 0;
    if (words.size() > 3 && !ParseInt(words[3], 1, 2304, &amount))
    {
        Fail(source, L"The amount must be a whole number from 1 to 2304.");
        return true;
    }
    if (words.size() > 4 && !ParseInt(words[4], 0, 32767, &data))
    {
        Fail(source, L"The data value must be a whole number from 0 to 32767.");
        return true;
    }
    PlayerVector targets;
    if (!ResolvePlayers(source, words, 1, targets))
        return true;
    Item* item = FindItem(source, words[2]);
    if (item == NULL)
        return true;

    for (size_t index = 0; index < targets.size(); ++index)
    {
        const PlayerPtr& target = targets[index];
        std::wstring name;
        int remaining = amount;
        while (remaining > 0)
        {
            not_null_ptr<ItemInstance> stack(new ItemInstance(item, 1, data));
            const int size = std::min(remaining, std::max(1, stack->getMaxStackSize()));
            stack->setCount(size);
            if (name.empty())
                name = stack->getHoverName();
            remaining -= size;
            const bool added = target->mInventory->add(stack);
            if (!added || stack->getCount() > 0)
            {
                boost::shared_ptr<ItemEntity> dropped = target->drop(stack, false);
                if (dropped)
                {
                    dropped->setNoPickUpDelay();
                    dropped->setOwner(target->getName());
                }
            }
        }
        target->mLevel->playSound(
            boost::shared_ptr<Player>(), target->mX, target->mY, target->mZ, SoundEvents::ITEM_PICKUP,
            SoundSource::eSoundSource_Players, 0.2f,
            ((target->getRandom()->nextFloat() - target->getRandom()->nextFloat()) * 0.7f + 1.0f) * 2.0f, 16.0f);
        target->mInventoryMenu->broadcastChanges();
        Tell(source, L"Given [" + name + L"] * " + Number(amount) + L" to " + target->getName());
    }
    return true;
}
