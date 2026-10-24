#include "game/command/chat/EnchantChatCommand.hpp"

#include "game/command/CommandArguments.hpp"
#include "game/command/TargetSelector.hpp"
#include "game/data/CompoundTag.hpp"
#include "game/data/ListTag.hpp"
#include "game/entity/mob/LivingEntity.hpp"
#include "game/item/Enchantment.hpp"
#include "game/item/ItemInstance.hpp"
#include "game/item/not_null_ptr.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/ui/AbstractContainerMenu.hpp"

using CommandArguments::Number;
using CommandArguments::ParseInt;

EnchantChatCommand::EnchantChatCommand()
    : ChatCommand(L"enchant", L"<targets> <enchantment> [level]", L"targets enchantment number", ePermission_Operator)
{
}

bool EnchantChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() < 3 || words.size() > 4)
        return false;
    LivingList targets;
    if (!ResolveLiving(source, words, 1, targets))
        return true;
    Enchantment* enchantment = FindEnchantment(source, words[2]);
    if (enchantment == NULL)
        return true;
    int level = enchantment->getMinLevel();
    if (words.size() > 3 && !ParseInt(words[3], enchantment->getMinLevel(), enchantment->getMaxLevel(), &level))
    {
        Fail(source, L"The level must be from " + Number(enchantment->getMinLevel()) + L" to "
                         + Number(enchantment->getMaxLevel()) + L".");
        return true;
    }
    const std::wstring name = EnchantmentName(enchantment);
    for (size_t index = 0; index < targets.size(); ++index)
    {
        const LivingPtr& target = targets[index];
        const std::wstring targetName = TargetSelector::DisplayName(target);
        not_null_ptr<ItemInstance> item = target->getMainHandItem();
        if (item->isEmpty())
        {
            Fail(source, targetName + L" is not holding an item.");
            continue;
        }
        if (!enchantment->canEnchant(item))
        {
            Fail(source, name + L" can't be added to " + item->getHoverName() + L".");
            continue;
        }
        Enchantment* conflict = NULL;
        ListTag* tags = item->hasTag() ? item->getEnchantmentTags() : NULL;
        for (int tag = 0; tags != NULL && tag < tags->size() && conflict == NULL; ++tag)
        {
            Enchantment* existing = Enchantment::byId(tags->getCompound(tag)->getShort(ItemInstance::TAG_ENCH_ID));
            if (existing != NULL && !existing->isCompatibleWith(enchantment))
                conflict = existing;
        }
        if (conflict != NULL)
        {
            Fail(source, name + L" can't be combined with " + EnchantmentName(conflict) + L".");
            continue;
        }
        item->enchant(enchantment, level);
        PlayerPtr player = boost::dynamic_pointer_cast<ServerPlayer>(target);
        if (player)
            player->mInventoryMenu->broadcastChanges();
        Tell(source,
             L"Enchanted " + targetName + L"'s " + item->getHoverName() + L" with " + name + L" " + Number(level));
    }
    return true;
}
