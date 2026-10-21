#include "game/command/ChatCommands.hpp"

#include <math.h>
#include <stdio.h>
#include <wctype.h>

#include <algorithm>
#include <vector>

#include <boost/pointer_cast.hpp>

#include "game/client/audio/SoundEvents.hpp"
#include "game/client/audio/SoundSource.hpp"
#include "game/command/CommandArguments.hpp"
#include "game/command/CommandDispatcher.hpp"
#include "game/command/CommandSource.hpp"
#include "game/command/DefaultGameModeCommand.hpp"
#include "game/command/GameDifficultyCommand.hpp"
#include "game/command/GameModeCommand.hpp"
#include "game/command/TargetSelector.hpp"
#include "game/command/TeleportCommand.hpp"
#include "game/command/TimeCommand.hpp"
#include "game/command/WeatherCommand.hpp"
#include "game/core/Difficulty.hpp"
#include "game/core/EntityType.hpp"
#include "game/core/GameType.hpp"
#include "game/core/LightningBolt.hpp"
#include "game/data/CompoundTag.hpp"
#include "game/data/ListTag.hpp"
#include "game/entity/mob/Entity.hpp"
#include "game/entity/mob/EntityIO.hpp"
#include "game/entity/mob/ItemEntity.hpp"
#include "game/entity/mob/LivingEntity.hpp"
#include "game/entity/mob/MobEffect.hpp"
#include "game/entity/mob/MobEffectInstance.hpp"
#include "game/item/Enchantment.hpp"
#include "game/item/Inventory.hpp"
#include "game/item/Item.hpp"
#include "game/item/ItemInstance.hpp"
#include "game/item/SpawnEggItem.hpp"
#include "game/item/not_null_ptr.hpp"
#include "game/network/CGameNetworkManager.hpp"
#include "game/network/ClientboundChatPacket.hpp"
#include "game/network/ClientboundSetSpawnPositionPacket.hpp"
#include "game/network/DisconnectPacket.hpp"
#include "game/network/GameCommandPacket.hpp"
#include "game/network/INetworkPlayer.hpp"
#include "game/network/ServerGamePacketListenerImpl.hpp"
#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerLevel.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/ui/AbstractContainerMenu.hpp"
#include "game/util/Random.hpp"
#include "game/util/ResourceLocation.hpp"
#include "game/world/Level.hpp"
#include "game/world/LevelData.hpp"
#include "game/world/block/Block.hpp"
#include "game/world/block/BlockPos.hpp"
#include "game/world/block/BlockState.hpp"
#include "game/world/block/Blocks.hpp"
#include "game/world/block/DynamicLiquidBlock.hpp"
#include "game/world/block/StaticLiquidBlock.hpp"
#include "game/world/generation/PlayerList.hpp"

namespace
{
typedef boost::shared_ptr<ServerPlayer> PlayerPtr;
typedef boost::shared_ptr<Entity> EntityPtr;
typedef boost::shared_ptr<LivingEntity> LivingPtr;
typedef std::vector<std::wstring> Words;
typedef std::vector<PlayerPtr> PlayerVector;
typedef std::vector<EntityPtr> EntityList;
typedef std::vector<LivingPtr> LivingList;

using CommandArguments::Lower;
using CommandArguments::Number;
using CommandArguments::ParseInt;
using CommandArguments::Pretty;
using CommandArguments::StripNamespace;

const int kFillLimit = 32768;
const size_t kHelpPageSize = 7;
const size_t kExecuteLimit = 4096;

struct Entry
{
    const wchar_t* name;
    const wchar_t* usage;
    const wchar_t* arguments;
    bool everyone;
    bool alias;
    bool (*run)(const CommandSource& source, const Words& words);
};

std::wstring LongNumber(long long value)
{
    wchar_t buffer[32];
    swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%lld", value);
    return buffer;
}

std::wstring Position(double x, double y, double z)
{
    wchar_t buffer[96];
    swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]), L"%.1f, %.1f, %.1f", x, y, z);
    return buffer;
}

std::wstring Position(const BlockPos& pos)
{
    return Number(pos.x) + L", " + Number(pos.y) + L", " + Number(pos.z);
}

std::wstring Count(int count, const wchar_t* one, const wchar_t* many)
{
    return Number(count) + L" " + (count == 1 ? one : many);
}

void Tell(const CommandSource& source, const std::wstring& text)
{
    if (source.output)
        ChatCommands::Reply(source.output, text);
}

void Fail(const CommandSource& source, const std::wstring& text)
{
    if (source.output)
        ChatCommands::Error(source.output, text);
}

std::wstring SourceName(const CommandSource& source)
{
    return TargetSelector::DisplayName(source.entity);
}

BlockPos SourceBlockPos(const CommandSource& source)
{
    return BlockPos(static_cast<int>(floor(source.x)), static_cast<int>(floor(source.y)),
                    static_cast<int>(floor(source.z)));
}

bool InOverworld(const CommandSource& source)
{
    return source.level == gMinecraftServer->getLevel(0);
}

void Perform(const CommandSource& source, boost::shared_ptr<GameCommandPacket> packet)
{
    gMinecraftServer->getCommandDispatcher()->performCommand(source.player, packet->command, packet->data);
}

void Broadcast(const std::wstring& text)
{
    std::wstring strings[2] = {std::wstring(), text};
    gMinecraftServer->getPlayers()->broadcastAll(boost::shared_ptr<Packet>(
        new ClientboundChatPacket(ClientboundChatPacket::eChatPacketMessage_PlayerChat, NULL, 0, strings, 2)));
}

bool ResolvePlayers(const CommandSource& source, const Words& words, size_t index, PlayerVector& players)
{
    players.clear();
    if (index >= words.size())
    {
        PlayerPtr self = boost::dynamic_pointer_cast<ServerPlayer>(source.entity);
        if (!self)
        {
            Fail(source, L"No player was found.");
            return false;
        }
        players.push_back(self);
        return true;
    }
    std::wstring error;
    if (!TargetSelector::Players(source, words[index], players, &error))
    {
        Fail(source, error);
        return false;
    }
    return true;
}

bool ResolveEntities(const CommandSource& source, const std::wstring& word, EntityList& entities)
{
    std::wstring error;
    if (!TargetSelector::Entities(source, word, entities, &error))
    {
        Fail(source, error);
        return false;
    }
    return true;
}

bool ResolveLiving(const CommandSource& source, const Words& words, size_t index, LivingList& living)
{
    living.clear();
    EntityList entities;
    if (index >= words.size())
        entities.push_back(source.entity);
    else if (!ResolveEntities(source, words[index], entities))
        return false;
    for (size_t entity = 0; entity < entities.size(); ++entity)
    {
        LivingPtr target = boost::dynamic_pointer_cast<LivingEntity>(entities[entity]);
        if (target)
            living.push_back(target);
    }
    if (living.empty())
    {
        Fail(source, L"No entity was found.");
        return false;
    }
    return true;
}

bool ParseGameType(const std::wstring& word, const GameType** type)
{
    const std::wstring value = Lower(word);
    if (value == L"survival" || value == L"s" || value == L"0")
        *type = GameType::SURVIVAL;
    else if (value == L"creative" || value == L"c" || value == L"1")
        *type = GameType::CREATIVE;
    else if (value == L"adventure" || value == L"a" || value == L"2")
        *type = GameType::ADVENTURE;
    else
        return false;
    return true;
}

Item* FindItem(const CommandSource& source, const std::wstring& word)
{
    const std::wstring name = StripNamespace(word);
    Item* item = NULL;
    int id = 0;
    if (Item::itemRegistry.containsKey(ResourceLocation(name)))
        item = Item::itemRegistry.get(ResourceLocation(name));
    else if (ParseInt(name, 1, 32767, &id))
        item = Item::byId(id);
    if (item == NULL || name == L"air")
    {
        Fail(source, L"There is no such item with name " + word + L".");
        return NULL;
    }
    return item;
}

Block* FindBlock(const CommandSource& source, const std::wstring& word)
{
    const std::wstring name = StripNamespace(word);
    Block* block = NULL;
    int id = 0;
    if (Block::blockRegistry->containsKey(ResourceLocation(name)))
        block = Block::blockRegistry->get(ResourceLocation(name));
    else if (ParseInt(name, 0, 4095, &id))
        block = Block::byId(id);
    if (block == NULL)
    {
        Fail(source, L"There is no such block with name " + word + L".");
        return NULL;
    }
    return block;
}

MobEffect* FindEffect(const CommandSource& source, const std::wstring& word)
{
    const std::wstring name = StripNamespace(word);
    MobEffect* effect = NULL;
    int id = 0;
    if (MobEffect::mobEffectRegistry->containsKey(ResourceLocation(name)))
        effect = MobEffect::byName(name);
    else if (ParseInt(name, 1, 255, &id))
        effect = MobEffect::byId(id);
    if (effect == NULL)
        Fail(source, L"There is no such mob effect with name " + word + L".");
    return effect;
}

std::wstring EffectName(MobEffect* effect)
{
    return Pretty(MobEffect::mobEffectRegistry->getKey(effect).getPath());
}

Enchantment* FindEnchantment(const CommandSource& source, const std::wstring& word)
{
    const std::wstring name = StripNamespace(word);
    Enchantment* enchantment = NULL;
    int id = 0;
    if (Enchantment::enchantmentRegistry->containsKey(ResourceLocation(name)))
        enchantment = Enchantment::byName(name);
    else if (ParseInt(name, 0, 255, &id))
        enchantment = Enchantment::byId(id);
    if (enchantment == NULL)
        Fail(source, L"There is no such enchantment with name " + word + L".");
    return enchantment;
}

std::wstring EnchantmentName(Enchantment* enchantment)
{
    return Pretty(Enchantment::enchantmentRegistry->getKey(enchantment).getPath());
}

bool ParsePosition(const CommandSource& source, const Words& words, size_t index, bool centre, double result[3])
{
    const double origin[3] = {source.x, source.y, source.z};
    std::wstring error;
    if (!CommandArguments::ParsePosition(words, index, origin, source.yRot, source.xRot, centre, result, &error))
    {
        Fail(source, error);
        return false;
    }
    return true;
}

bool ParseBlockPos(const CommandSource& source, const Words& words, size_t index, BlockPos* pos)
{
    double result[3];
    if (!ParsePosition(source, words, index, false, result))
        return false;
    *pos = BlockPos(static_cast<int>(floor(result[0])), static_cast<int>(floor(result[1])),
                    static_cast<int>(floor(result[2])));
    return true;
}

bool ParseFlag(const std::wstring& word, bool* value)
{
    const std::wstring lower = Lower(word);
    if (lower == L"true")
        *value = true;
    else if (lower == L"false")
        *value = false;
    else
        return false;
    return true;
}

bool Help(const CommandSource& source, const Words& words);
bool Execute(const CommandSource& source, const Words& words);

bool Time(const CommandSource& source, const Words& words)
{
    if (words.size() < 2 || words.size() > 3)
        return false;
    const std::wstring operation = words.size() == 2 ? std::wstring(L"set") : Lower(words[1]);
    const std::wstring value = Lower(words[words.size() - 1]);
    ServerLevel* level = gMinecraftServer->getLevel(0);
    if (operation == L"query")
    {
        if (value == L"daytime")
            Tell(source, L"The time is " + LongNumber(level->getDayTime() % 24000));
        else if (value == L"gametime")
            Tell(source, L"The time is " + LongNumber(level->getGameTime()));
        else if (value == L"day")
            Tell(source, L"The day is " + LongNumber(level->getDayTime() / 24000));
        else
            return false;
        return true;
    }

    double number = 0.0;
    const bool numeric = CommandArguments::ParseNumber(value, &number) && number >= 0.0 && number <= 2147483647.0;
    long long time = 0;
    if (operation == L"set")
    {
        if (value == L"day")
            time = 1000;
        else if (value == L"noon")
            time = 6000;
        else if (value == L"night")
            time = 13000;
        else if (value == L"midnight")
            time = 18000;
        else if (numeric)
            time = static_cast<long long>(number);
        else
            return false;
    }
    else if (operation == L"add" && numeric)
        time = level->getDayTime() + static_cast<long long>(number);
    else
        return false;
    Perform(source, TimeCommand::preparePacket(time));
    return true;
}

bool Weather(const CommandSource& source, const Words& words)
{
    const std::wstring value = words.size() == 2 ? Lower(words[1]) : std::wstring();
    unsigned char mode;
    if (value == L"clear")
        mode = 0;
    else if (value == L"rain")
        mode = 1;
    else if (value == L"thunder")
        mode = 2;
    else
        return false;
    Perform(source, WeatherCommand::preparePacket(mode));
    return true;
}

bool GameMode(const CommandSource& source, const Words& words)
{
    const GameType* type = NULL;
    if (words.size() < 2 || words.size() > 3 || !ParseGameType(words[1], &type))
        return false;
    PlayerVector targets;
    if (!ResolvePlayers(source, words, 2, targets))
        return true;
    for (size_t index = 0; index < targets.size(); ++index)
        Perform(source, GameModeCommand::preparePacket(targets[index]->getXuid(), (unsigned char)type->getId()));
    return true;
}

bool Teleport(const CommandSource& source, const Words& words)
{
    const size_t arguments = words.size() - 1;
    if (arguments < 1 || arguments > 4)
        return false;

    EntityList moved;
    size_t next = 1;
    if (arguments == 2 || arguments == 4)
    {
        if (!ResolveEntities(source, words[1], moved))
            return true;
        next = 2;
    }
    else
        moved.push_back(source.entity);

    EntityPtr destination;
    if (words.size() - next == 1)
    {
        EntityList found;
        if (!ResolveEntities(source, words[next], found))
            return true;
        if (found.size() != 1)
        {
            Fail(source, L"The destination matched more than one entity.");
            return true;
        }
        destination = found[0];
    }

    double target[3] = {0.0, 0.0, 0.0};
    if (!destination && !ParsePosition(source, words, next, true, target))
        return true;
    Level* level = destination ? destination->mLevel : source.level;
    const std::wstring where =
        destination ? TargetSelector::DisplayName(destination) : Position(target[0], target[1], target[2]);

    int teleported = 0;
    for (size_t index = 0; index < moved.size(); ++index)
    {
        const EntityPtr& entity = moved[index];
        if (entity->mLevel != level)
        {
            Fail(source, TargetSelector::DisplayName(entity) + L" is in another dimension.");
            continue;
        }
        const double x = destination ? destination->mX : target[0];
        const double y = destination ? destination->mY : target[1];
        const double z = destination ? destination->mZ : target[2];
        PlayerPtr movedPlayer = boost::dynamic_pointer_cast<ServerPlayer>(entity);
        PlayerPtr destinationPlayer = boost::dynamic_pointer_cast<ServerPlayer>(destination);
        if (movedPlayer && destinationPlayer)
            Perform(source, TeleportCommand::preparePacket(movedPlayer->getXuid(), destinationPlayer->getXuid()));
        else if (movedPlayer)
            Perform(source, TeleportCommand::preparePacket(movedPlayer->getXuid(), (float)x, (float)y, (float)z));
        else
            entity->moveTo(x, y, z, entity->mYRot, entity->mXRot);
        ++teleported;
    }
    if (teleported == 1 && moved.size() == 1)
        Tell(source, L"Teleported " + TargetSelector::DisplayName(moved[0]) + L" to " + where + L".");
    else if (teleported > 0)
        Tell(source, L"Teleported " + Count(teleported, L"entity", L"entities") + L" to " + where + L".");
    return true;
}

bool Kill(const CommandSource& source, const Words& words)
{
    if (words.size() > 2)
        return false;
    EntityList targets;
    if (words.size() == 2)
    {
        if (!ResolveEntities(source, words[1], targets))
            return true;
    }
    else
        targets.push_back(source.entity);
    for (size_t index = 0; index < targets.size(); ++index)
        targets[index]->kill();
    if (targets.size() == 1)
        Tell(source, L"Killed " + TargetSelector::DisplayName(targets[0]));
    else
        Tell(source, L"Killed " + Count(static_cast<int>(targets.size()), L"entity", L"entities"));
    return true;
}

bool Give(const CommandSource& source, const Words& words)
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

bool Clear(const CommandSource& source, const Words& words)
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
            Tell(source, target->getName() + L" has " + Count(removed, L"item", L"items") + L" that match the criteria");
        else if (removed > 0)
            Tell(source, L"Cleared the inventory of " + target->getName() + L", removing "
                             + Count(removed, L"item", L"items"));
        else
            Fail(source, L"Could not clear the inventory of " + target->getName() + L", no items to remove");
    }
    return true;
}

bool Effect(const CommandSource& source, const Words& words)
{
    if (words.size() < 3 || words.size() > 6)
        return false;
    LivingList targets;
    if (Lower(words[2]) == L"clear")
    {
        if (words.size() != 3)
            return false;
        if (!ResolveLiving(source, words, 1, targets))
            return true;
        for (size_t index = 0; index < targets.size(); ++index)
        {
            const LivingPtr& target = targets[index];
            const std::wstring targetName = TargetSelector::DisplayName(target);
            std::vector<MobEffectInstance*>* active = target->getActiveEffects();
            const bool none = active->empty();
            delete active;
            if (none)
            {
                Fail(source, L"Couldn't take any effects from " + targetName + L" as they do not have any");
                continue;
            }
            target->removeAllEffects();
            Tell(source, L"Took all effects from " + targetName);
        }
        return true;
    }

    MobEffect* effect = FindEffect(source, words[2]);
    if (effect == NULL)
        return true;
    const bool instant = effect->isInstantenous();
    int duration = instant ? 1 : 30;
    int amplifier = 0;
    bool hideParticles = false;
    if (words.size() > 3 && !ParseInt(words[3], 0, 1000000, &duration))
    {
        Fail(source, L"The seconds must be a whole number from 0 to 1000000.");
        return true;
    }
    if (words.size() > 4 && !ParseInt(words[4], 0, 255, &amplifier))
    {
        Fail(source, L"The amplifier must be a whole number from 0 to 255.");
        return true;
    }
    if (words.size() > 5 && !ParseFlag(words[5], &hideParticles))
        return false;
    if (!ResolveLiving(source, words, 1, targets))
        return true;

    const std::wstring name = EffectName(effect);
    int given = 0;
    for (size_t index = 0; index < targets.size(); ++index)
    {
        const LivingPtr& target = targets[index];
        const std::wstring targetName = TargetSelector::DisplayName(target);
        if (duration == 0)
        {
            if (!target->hasEffect(effect))
            {
                if (targets.size() == 1)
                    Fail(source, L"Couldn't take " + name + L" from " + targetName + L" as they do not have the effect");
                continue;
            }
            target->removeEffect(effect);
            ++given;
            if (targets.size() == 1)
                Tell(source, L"Took " + name + L" from " + targetName);
            continue;
        }
        MobEffectInstance* instance =
            new MobEffectInstance(effect, instant ? duration : duration * 20, amplifier, false, !hideParticles);
        if (!target->canBeAffected(instance))
        {
            delete instance;
            if (targets.size() == 1)
                Fail(source, targetName + L" can't be given " + name);
            continue;
        }
        const bool had = target->hasEffect(effect);
        target->addEffect(instance, boost::shared_ptr<Entity>());
        if (had)
            delete instance;
        ++given;
        if (targets.size() == 1)
            Tell(source, L"Given " + name + L" * " + Number(amplifier) + L" to " + targetName
                             + (instant ? std::wstring() : L" for " + Number(duration) + L" seconds"));
    }
    if (targets.size() > 1)
    {
        if (given == 0)
            Fail(source, L"None of the " + Count(static_cast<int>(targets.size()), L"entity", L"entities")
                             + (duration == 0 ? L" had " : L" could be given ") + name);
        else if (duration == 0)
            Tell(source, L"Took " + name + L" from " + Count(given, L"entity", L"entities"));
        else
            Tell(source, L"Given " + name + L" * " + Number(amplifier) + L" to " + Count(given, L"entity", L"entities")
                             + (instant ? std::wstring() : L" for " + Number(duration) + L" seconds"));
    }
    return true;
}

bool Enchant(const CommandSource& source, const Words& words)
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
        Tell(source, L"Enchanted " + targetName + L"'s " + item->getHoverName() + L" with " + name + L" "
                         + Number(level));
    }
    return true;
}

void AddPoints(const PlayerPtr& player, int amount)
{
    if (amount >= 0)
    {
        player->increaseXp(amount);
        return;
    }
    int remove = -amount;
    player->mTotalExperience = std::max(0, player->mTotalExperience - remove);
    while (remove > 0)
    {
        const int needed = player->getXpNeededForNextLevel();
        const int have = static_cast<int>(player->mExperienceProgress * needed);
        if (remove <= have)
        {
            player->mExperienceProgress = static_cast<float>(have - remove) / needed;
            break;
        }
        remove -= have;
        if (player->mExperienceLevel == 0)
        {
            player->mExperienceProgress = 0.0f;
            break;
        }
        player->giveExperienceLevels(-1);
        player->mExperienceProgress = 1.0f;
    }
    player->giveExperienceLevels(0);
}

int Points(const PlayerPtr& player)
{
    return static_cast<int>(player->mExperienceProgress * player->getXpNeededForNextLevel());
}

bool ParseExperienceUnit(const Words& words, size_t index, bool* levels)
{
    *levels = false;
    if (index >= words.size())
        return true;
    const std::wstring unit = Lower(words[index]);
    if (unit == L"levels")
        *levels = true;
    else if (unit != L"points")
        return false;
    return index + 1 == words.size();
}

bool Experience(const CommandSource& source, const Words& words)
{
    if (words.size() < 2)
        return false;
    const std::wstring operation = Lower(words[1]);
    PlayerVector targets;
    bool levels = false;
    int amount = 0;

    if (operation == L"query")
    {
        if (words.size() != 4 || !ParseExperienceUnit(words, 3, &levels))
            return false;
        if (!ResolvePlayers(source, words, 2, targets))
            return true;
        for (size_t index = 0; index < targets.size(); ++index)
        {
            const PlayerPtr& target = targets[index];
            if (levels)
                Tell(source, target->getName() + L" has " + Count(target->mExperienceLevel, L"experience level",
                                                                 L"experience levels"));
            else
                Tell(source, target->getName() + L" has " + Count(Points(target), L"experience point",
                                                                 L"experience points"));
        }
        return true;
    }

    if (operation == L"add" || operation == L"set")
    {
        const bool set = operation == L"set";
        if (words.size() < 4 || words.size() > 5 || !ParseExperienceUnit(words, 4, &levels))
            return false;
        if (!ParseInt(words[3], set ? 0 : -1000000, 1000000, &amount))
        {
            Fail(source, set ? L"The amount must be a whole number of 0 or more."
                             : L"The amount must be a whole number.");
            return true;
        }
        if (!ResolvePlayers(source, words, 2, targets))
            return true;
        for (size_t index = 0; index < targets.size(); ++index)
        {
            const PlayerPtr& target = targets[index];
            if (set && levels)
                target->giveExperienceLevels(amount - target->mExperienceLevel);
            else if (set)
            {
                const int needed = target->getXpNeededForNextLevel();
                if (amount >= needed)
                {
                    Fail(source, L"Points must be below " + Number(needed) + L" for " + target->getName()
                                     + L"'s current level.");
                    continue;
                }
                target->mTotalExperience = std::max(0, target->mTotalExperience + amount - Points(target));
                target->mExperienceProgress = static_cast<float>(amount) / needed;
                target->giveExperienceLevels(0);
            }
            else if (levels)
                target->giveExperienceLevels(amount);
            else
                AddPoints(target, amount);
            const std::wstring unit = levels ? L" experience levels" : L" experience points";
            if (set)
                Tell(source, L"Set " + target->getName() + L"'s" + unit + L" to " + Number(amount));
            else
                Tell(source, (amount < 0 ? L"Took " + Number(-amount) + unit + L" from "
                                         : L"Gave " + Number(amount) + unit + L" to ")
                                 + target->getName());
        }
        return true;
    }

    if (words.size() > 3)
        return false;
    std::wstring amountText = words[1];
    levels = !amountText.empty() && towlower(amountText[amountText.size() - 1]) == L'l';
    if (levels)
        amountText.erase(amountText.size() - 1);
    if (!ParseInt(amountText, levels ? -1000000 : 0, 1000000, &amount))
        return false;
    if (!ResolvePlayers(source, words, 2, targets))
        return true;
    for (size_t index = 0; index < targets.size(); ++index)
    {
        const PlayerPtr& target = targets[index];
        if (levels)
            target->giveExperienceLevels(amount);
        else
            target->increaseXp(amount);
        const std::wstring unit = levels ? L" experience levels" : L" experience points";
        Tell(source, (amount < 0 ? L"Took " + Number(-amount) + unit + L" from "
                                 : L"Gave " + Number(amount) + unit + L" to ")
                         + target->getName());
    }
    return true;
}

bool Spawnpoint(const CommandSource& source, const Words& words)
{
    if (words.size() != 1 && words.size() != 2 && words.size() != 5)
        return false;
    if (!InOverworld(source))
    {
        Fail(source, L"Spawn points can only be set in the Overworld.");
        return true;
    }
    BlockPos pos = SourceBlockPos(source);
    if (words.size() == 5 && !ParseBlockPos(source, words, 2, &pos))
        return true;
    PlayerVector targets;
    if (!ResolvePlayers(source, words, 1, targets))
        return true;
    for (size_t index = 0; index < targets.size(); ++index)
    {
        targets[index]->setRespawnPosition(new BlockPos(pos), true);
        Tell(source, L"Set " + targets[index]->getName() + L"'s spawn point to (" + Position(pos) + L")");
    }
    return true;
}

bool GameDifficulty(const CommandSource& source, const Words& words)
{
    if (words.size() != 2)
        return false;
    const std::wstring value = Lower(words[1]);
    int id = -1;
    if (value == L"peaceful" || value == L"p" || value == L"0")
        id = 0;
    else if (value == L"easy" || value == L"e" || value == L"1")
        id = 1;
    else if (value == L"normal" || value == L"n" || value == L"2")
        id = 2;
    else if (value == L"hard" || value == L"h" || value == L"3")
        id = 3;
    else
        return false;
    Perform(source, GameDifficultyCommand::preparePacket(id));
    return true;
}

bool DefaultGameMode(const CommandSource& source, const Words& words)
{
    const GameType* type = NULL;
    if (words.size() != 2 || !ParseGameType(words[1], &type))
        return false;
    Perform(source, DefaultGameModeCommand::preparePacket((unsigned char)type->getId()));
    return true;
}

bool SetWorldSpawn(const CommandSource& source, const Words& words)
{
    if (words.size() != 1 && words.size() != 4)
        return false;
    if (!InOverworld(source))
    {
        Fail(source, L"The world spawn can only be set in the Overworld.");
        return true;
    }
    BlockPos pos = SourceBlockPos(source);
    if (words.size() == 4 && !ParseBlockPos(source, words, 1, &pos))
        return true;
    gMinecraftServer->getLevel(0)->setSpawnPos(pos);
    gMinecraftServer->getPlayers()->broadcastAll(
        boost::shared_ptr<Packet>(new ClientboundSetSpawnPositionPacket(pos.x, pos.y, pos.z)));
    Tell(source, L"Set the world spawn point to (" + Position(pos) + L")");
    return true;
}

bool ToggleDownfall(const CommandSource& source, const Words& words)
{
    if (words.size() != 1)
        return false;
    LevelData* data = gMinecraftServer->getLevel(0)->getLevelData();
    data->setRaining(!data->isRaining());
    Tell(source, L"Toggled downfall");
    return true;
}

bool Seed(const CommandSource& source, const Words& words)
{
    if (words.size() != 1)
        return false;
    Tell(source, L"Seed: [§a" + LongNumber(gMinecraftServer->getLevel(0)->getSeed()) + L"§r]");
    return true;
}

bool Summon(const CommandSource& source, const Words& words)
{
    if (words.size() != 2 && words.size() != 5)
        return false;
    double x = source.x;
    double y = source.y;
    double z = source.z;
    if (words.size() == 5)
    {
        double result[3];
        if (!ParsePosition(source, words, 2, true, result))
            return true;
        x = result[0];
        y = result[1];
        z = result[2];
    }
    Level* level = source.level;
    const BlockPos pos(x, y, z);
    if (!level->isInWorldBounds(pos) || !level->hasChunkAt(pos))
    {
        Fail(source, L"Cannot summon the object out of the world");
        return true;
    }

    const std::wstring type = StripNamespace(words[1]);
    if (type == L"lightning_bolt" || type == L"lightningbolt")
    {
        level->addGlobalEntity(boost::shared_ptr<Entity>(new LightningBolt(level, x, y, z, false)));
        Tell(source, L"Object successfully summoned");
        return true;
    }
    const ResourceLocation location(type);
    if (type == L"player" || !EntityIO::isValidEntityType(location))
    {
        Fail(source, L"Unknown entity type " + words[1] + L".");
        return true;
    }

    EntityPtr entity;
    if (EntityIO::sSpawnInfo.find(location) != EntityIO::sSpawnInfo.end())
    {
        int error = 0;
        entity = SpawnEggItem::spawnMobAt(level, location, x, y, z, &error, L"");
        if (!entity && error != 0)
        {
            if (source.output)
                SpawnEggItem::DisplaySpawnError(source.output, error);
            return true;
        }
    }
    else
    {
        entity = EntityIO::newEntity(location, level);
        if (entity)
        {
            entity->moveTo(x, y, z, source.yRot, 0.0f);
            if (!level->addEntity(entity))
                entity.reset();
        }
    }
    if (!entity)
    {
        Fail(source, L"Unable to summon object");
        return true;
    }
    Tell(source, L"Object successfully summoned");
    return true;
}

bool ParseBlockData(const CommandSource& source, const Words& words, size_t index, int* data)
{
    *data = 0;
    if (index < words.size() && !ParseInt(words[index], 0, 15, data))
    {
        Fail(source, L"The data value must be a whole number from 0 to 15.");
        return false;
    }
    return true;
}

Block* StillLiquid(Block* block)
{
    if (block == Blocks::flowing_water)
        return Blocks::water;
    if (block == Blocks::flowing_lava)
        return Blocks::lava;
    return block;
}

bool SetBlock(const CommandSource& source, const Words& words)
{
    if (words.size() < 5 || words.size() > 7)
        return false;
    BlockPos pos;
    if (!ParseBlockPos(source, words, 1, &pos))
        return true;
    Block* block = FindBlock(source, words[4]);
    int data = 0;
    if (block == NULL || !ParseBlockData(source, words, 5, &data))
        return true;
    const std::wstring mode = words.size() > 6 ? Lower(words[6]) : std::wstring(L"replace");
    if (mode != L"replace" && mode != L"destroy" && mode != L"keep")
        return false;
    Level* level = source.level;
    if (!level->isInWorldBounds(pos) || !level->hasChunkAt(pos))
    {
        Fail(source, L"Cannot place block outside of the world");
        return true;
    }
    if (mode == L"keep" && !level->isEmptyBlock(pos))
    {
        Fail(source, L"Could not set the block");
        return true;
    }
    if (mode == L"destroy")
        level->destroyBlock(pos, true);
    if (!level->setBlock(pos, block->getBlockState(data), Block::UPDATE_ALL, false))
    {
        Fail(source, L"Could not set the block");
        return true;
    }
    Tell(source, L"Block placed");
    return true;
}

bool Fill(const CommandSource& source, const Words& words)
{
    if (words.size() < 8 || words.size() > 12)
        return false;
    BlockPos first;
    BlockPos second;
    if (!ParseBlockPos(source, words, 1, &first) || !ParseBlockPos(source, words, 4, &second))
        return true;
    Block* block = FindBlock(source, words[7]);
    int data = 0;
    if (block == NULL || !ParseBlockData(source, words, 8, &data))
        return true;
    const std::wstring mode = words.size() > 9 ? Lower(words[9]) : std::wstring(L"replace");
    if (mode != L"replace" && mode != L"destroy" && mode != L"keep" && mode != L"hollow" && mode != L"outline")
        return false;
    Block* filter = NULL;
    int filterData = -1;
    if (words.size() > 10)
    {
        if (mode != L"replace")
            return false;
        filter = FindBlock(source, words[10]);
        if (filter == NULL)
            return true;
        filter = StillLiquid(filter);
        if (words.size() > 11 && !ParseInt(words[11], -1, 15, &filterData))
        {
            Fail(source, L"The data value must be a whole number from -1 to 15.");
            return true;
        }
    }

    const int x0 = std::min(first.x, second.x);
    const int y0 = std::min(first.y, second.y);
    const int z0 = std::min(first.z, second.z);
    const int x1 = std::max(first.x, second.x);
    const int y1 = std::max(first.y, second.y);
    const int z1 = std::max(first.z, second.z);
    const long long volume = (long long)(x1 - x0 + 1) * (y1 - y0 + 1) * (z1 - z0 + 1);
    if (volume > kFillLimit)
    {
        Fail(source, L"Too many blocks in the specified area (" + LongNumber(volume) + L" > " + Number(kFillLimit)
                         + L")");
        return true;
    }
    Level* level = source.level;
    if (!level->isInWorldBounds(x0, y0, z0) || !level->isInWorldBounds(x1, y1, z1))
    {
        Fail(source, L"Cannot place blocks outside of the world");
        return true;
    }
    for (int chunkX = x0 & ~15; chunkX <= x1; chunkX += 16)
    {
        for (int chunkZ = z0 & ~15; chunkZ <= z1; chunkZ += 16)
        {
            if (!level->hasChunkAt(BlockPos(chunkX, y0, chunkZ)))
            {
                Fail(source, L"Cannot place blocks outside of the world");
                return true;
            }
        }
    }

    const BlockState* state = block->getBlockState(data);
    const BlockState* air = Blocks::air->defaultBlockState();
    int changed = 0;
    for (int y = y0; y <= y1; ++y)
    {
        for (int z = z0; z <= z1; ++z)
        {
            for (int x = x0; x <= x1; ++x)
            {
                const BlockPos pos(x, y, z);
                const bool edge = x == x0 || x == x1 || y == y0 || y == y1 || z == z0 || z == z1;
                const BlockState* placed = state;
                if (mode == L"hollow" && !edge)
                    placed = air;
                else if (mode == L"outline" && !edge)
                    continue;
                else if (mode == L"keep" && !level->isEmptyBlock(pos))
                    continue;
                else if (filter != NULL)
                {
                    const BlockState* current = level->getBlockState(pos);
                    Block* currentBlock = current->getBlock();
                    if (StillLiquid(currentBlock) != filter
                        || (filterData >= 0 && currentBlock->convertBlockStateToLegacyData(current) != filterData))
                        continue;
                }
                if (mode == L"destroy")
                    level->destroyBlock(pos, true);
                if (level->setBlock(pos, placed, Block::UPDATE_ALL, false))
                    ++changed;
            }
        }
    }
    if (changed == 0)
    {
        Fail(source, L"No blocks filled");
        return true;
    }
    Tell(source, Count(changed, L"block", L"blocks") + L" filled");
    return true;
}

bool Say(const CommandSource& source, const Words& words)
{
    if (words.size() < 2)
        return false;
    const std::wstring message = ChatCommands::FormatMessage(CommandArguments::Join(words, 1));
    if (message.empty())
        return false;
    Broadcast(L"[" + SourceName(source) + L"] " + message);
    return true;
}

bool Me(const CommandSource& source, const Words& words)
{
    if (words.size() < 2)
        return false;
    const std::wstring message = ChatCommands::FormatMessage(CommandArguments::Join(words, 1));
    if (message.empty())
        return false;
    Broadcast(L"* " + SourceName(source) + L" " + message);
    return true;
}

bool Message(const CommandSource& source, const Words& words)
{
    if (words.size() < 3)
        return false;
    const std::wstring message = ChatCommands::FormatMessage(CommandArguments::Join(words, 2));
    if (message.empty())
        return false;
    PlayerVector targets;
    if (!ResolvePlayers(source, words, 1, targets))
        return true;
    const std::wstring name = SourceName(source);
    for (size_t index = 0; index < targets.size(); ++index)
    {
        const PlayerPtr& target = targets[index];
        if (target == source.entity)
        {
            Fail(source, L"You can't send a private message to yourself!");
            continue;
        }
        ChatCommands::Reply(target, L"§7§o" + name + L" whispers to you: " + message);
        Tell(source, L"§7§oYou whisper to " + target->getName() + L": " + message);
    }
    return true;
}

bool List(const CommandSource& source, const Words& words)
{
    if (words.size() != 1)
        return false;
    std::vector<PlayerPtr>* players = gMinecraftServer->getPlayers()->getPlayers();
    Tell(source, L"There are §e" + Number(static_cast<int>(players->size())) + L"§r/§e"
                     + Number(static_cast<int>(CGameNetworkManager::sInstance.GetMaxPlayers()))
                     + L"§r players online:");
    Tell(source, TargetSelector::Names(*players));
    return true;
}

bool Kick(const CommandSource& source, const Words& words)
{
    if (words.size() < 2)
        return false;
    PlayerPtr target = gMinecraftServer->getPlayers()->getPlayerByNameIgnoringCase(words[1]);
    if (!target)
    {
        Fail(source, L"There is no player called " + words[1] + L".");
        return true;
    }
    if (!target->mConnection)
    {
        Fail(source, target->getName() + L" can't be kicked.");
        return true;
    }
    INetworkPlayer* network = target->mConnection->getNetworkPlayer();
    if (network == NULL || network->IsHost() || network->IsLocal())
    {
        Fail(source, L"You can't kick the host or a player playing on the host's console.");
        return true;
    }
    const std::wstring reason = ChatCommands::FormatMessage(CommandArguments::Join(words, 2));
    const std::wstring name = target->getName();
    target->mConnection->kick();
    Tell(source, L"Kicked " + name + L" from the game" + (reason.empty() ? std::wstring() : L": '" + reason + L"§r'"));
    return true;
}

const Entry kEntries[] = {
    {L"help", L"[page|command]", L"command", true, false, Help},
    {L"?", L"[page|command]", L"command", true, true, Help},
    {L"execute", L"<as|at|positioned|if|unless|run> ...", L"execute", false, false, Execute},
    {L"time", L"<set|add|query> <value>", L"set|add|query time", false, false, Time},
    {L"weather", L"<clear|rain|thunder>", L"clear|rain|thunder", false, false, Weather},
    {L"gamemode", L"<mode> [player]", L"mode targets", false, false, GameMode},
    {L"tp", L"[targets] <destination|x y z>", L"targets targets coord coord", false, false, Teleport},
    {L"teleport", L"[targets] <destination|x y z>", L"targets targets coord coord", false, true, Teleport},
    {L"kill", L"[targets]", L"targets", false, false, Kill},
    {L"give", L"<player> <item> [amount] [data]", L"targets item number number", false, false, Give},
    {L"clear", L"[player] [item] [data] [maxCount]", L"targets item number number", false, false, Clear},
    {L"effect", L"<targets> <effect|clear> [seconds] [amplifier] [hideParticles]",
     L"targets effect number number true|false", false, false, Effect},
    {L"enchant", L"<targets> <enchantment> [level]", L"targets enchantment number", false, false, Enchant},
    {L"xp", L"<add|set|query> <targets> <amount> [points|levels]", L"add|set|query targets xpvalue points|levels",
     false, false, Experience},
    {L"experience", L"<add|set|query> <targets> <amount> [points|levels]",
     L"add|set|query targets xpvalue points|levels", false, true, Experience},
    {L"spawnpoint", L"[player] [x y z]", L"targets coord coord coord", false, false, Spawnpoint},
    {L"difficulty", L"<peaceful|easy|normal|hard>", L"peaceful|easy|normal|hard", false, false, GameDifficulty},
    {L"defaultgamemode", L"<mode>", L"mode", false, false, DefaultGameMode},
    {L"setworldspawn", L"[x y z]", L"coord coord coord", false, false, SetWorldSpawn},
    {L"toggledownfall", L"", L"", false, false, ToggleDownfall},
    {L"seed", L"", L"", false, false, Seed},
    {L"summon", L"<entity> [x y z]", L"entity coord coord coord", false, false, Summon},
    {L"setblock", L"<x y z> <block> [data] [replace|destroy|keep]",
     L"coord coord coord block number replace|destroy|keep", false, false, SetBlock},
    {L"fill", L"<x1 y1 z1> <x2 y2 z2> <block> [data] [mode] [replaceBlock] [replaceData]",
     L"coord coord coord coord coord coord block number replace|destroy|keep|hollow|outline block number", false, false,
     Fill},
    {L"say", L"<message>", L"text", false, false, Say},
    {L"me", L"<action>", L"text", true, false, Me},
    {L"msg", L"<player> <message>", L"targets text", true, false, Message},
    {L"tell", L"<player> <message>", L"targets text", true, true, Message},
    {L"w", L"<player> <message>", L"targets text", true, true, Message},
    {L"list", L"", L"", true, false, List},
    {L"kick", L"<player> [reason]", L"player text", false, false, Kick},
};

const size_t kEntryCount = sizeof(kEntries) / sizeof(kEntries[0]);

std::wstring UsageLine(const Entry& entry)
{
    std::wstring line = L"§e/" + std::wstring(entry.name);
    if (entry.usage[0] != 0)
        line += L" §7" + std::wstring(entry.usage);
    return line;
}

std::wstring UsageError(const Entry& entry)
{
    return L"Usage: /" + std::wstring(entry.name)
           + (entry.usage[0] != 0 ? L" " + std::wstring(entry.usage) : std::wstring());
}

const Entry* FindEntry(const std::wstring& name)
{
    for (size_t index = 0; index < kEntryCount; ++index)
    {
        if (name == kEntries[index].name)
            return &kEntries[index];
    }
    return NULL;
}

bool Help(const CommandSource& source, const Words& words)
{
    if (words.size() > 2)
        return false;
    int page = 1;
    if (words.size() == 2 && !ParseInt(words[1], 1, 1000, &page))
    {
        const std::wstring name = Lower(words[1][0] == L'/' ? words[1].substr(1) : words[1]);
        const Entry* entry = FindEntry(name);
        if (entry == NULL)
        {
            Fail(source, L"Unknown command /" + name + L". Type /help for the commands.");
            return true;
        }
        Tell(source, UsageLine(*entry));
        if (name == L"execute")
        {
            Tell(source, L"§7  as <targets>, at <targets>, positioned <x y z>, positioned as <targets>,");
            Tell(source, L"§7  if entity <targets>, unless entity <targets>, run <command>");
        }
        return true;
    }

    std::vector<const Entry*> listed;
    for (size_t index = 0; index < kEntryCount; ++index)
    {
        if (!kEntries[index].alias)
            listed.push_back(&kEntries[index]);
    }
    const int pages = static_cast<int>((listed.size() + kHelpPageSize - 1) / kHelpPageSize);
    if (page > pages)
        page = pages;
    Tell(source, L"§2--- Showing help page " + Number(page) + L" of " + Number(pages) + L" (/help <page>) ---");
    for (size_t index = (page - 1) * kHelpPageSize; index < listed.size() && index < page * kHelpPageSize; ++index)
        Tell(source, UsageLine(*listed[index]));
    return true;
}

bool Fork(const CommandSource& source, const std::vector<CommandSource>& from, const std::wstring& word,
          bool moveTo, bool rotate, bool becomeEntity, std::vector<CommandSource>& to)
{
    to.clear();
    for (size_t index = 0; index < from.size(); ++index)
    {
        EntityList found;
        std::wstring error;
        if (!TargetSelector::Select(from[index], word, found, &error))
        {
            Fail(source, error);
            return false;
        }
        for (size_t entity = 0; entity < found.size(); ++entity)
        {
            CommandSource next = from[index];
            const EntityPtr& target = found[entity];
            if (becomeEntity)
            {
                next.entity = target;
                next.output = boost::dynamic_pointer_cast<ServerPlayer>(target);
            }
            if (moveTo)
            {
                next.level = target->mLevel;
                next.x = target->mX;
                next.y = target->mY;
                next.z = target->mZ;
            }
            if (rotate)
            {
                next.yRot = target->mYRot;
                next.xRot = target->mXRot;
            }
            to.push_back(next);
            if (to.size() > kExecuteLimit)
            {
                Fail(source, L"That would run the command more than " + Number(static_cast<int>(kExecuteLimit))
                                 + L" times.");
                return false;
            }
        }
    }
    return true;
}

bool Execute(const CommandSource& source, const Words& words)
{
    if (words.size() < 2)
        return false;
    std::vector<CommandSource> sources(1, source);
    std::vector<CommandSource> next;
    bool tested = false;
    size_t index = 1;
    while (index < words.size())
    {
        const std::wstring part = Lower(words[index]);
        tested = false;
        if (part == L"run")
        {
            if (index + 1 >= words.size())
                return false;
            const std::wstring name = Lower(words[index + 1]);
            const Entry* entry = FindEntry(name);
            if (entry == NULL)
            {
                Fail(source, L"Unknown command /" + name + L". Type /help for the commands.");
                return true;
            }
            const Words rest(words.begin() + index + 1, words.end());
            for (size_t run = 0; run < sources.size(); ++run)
            {
                if (!entry->run(sources[run], rest))
                {
                    Fail(source, UsageError(*entry));
                    return true;
                }
            }
            return true;
        }
        if (part == L"as" || part == L"at")
        {
            if (index + 1 >= words.size())
                return false;
            const bool as = part == L"as";
            if (!Fork(source, sources, words[index + 1], !as, !as, as, next))
                return true;
            sources.swap(next);
            index += 2;
            continue;
        }
        if (part == L"positioned")
        {
            if (index + 2 < words.size() && Lower(words[index + 1]) == L"as")
            {
                if (!Fork(source, sources, words[index + 2], true, false, false, next))
                    return true;
                sources.swap(next);
                index += 3;
                continue;
            }
            if (index + 3 >= words.size())
                return false;
            for (size_t at = 0; at < sources.size(); ++at)
            {
                double result[3];
                const double origin[3] = {sources[at].x, sources[at].y, sources[at].z};
                std::wstring error;
                if (!CommandArguments::ParsePosition(words, index + 1, origin, sources[at].yRot, sources[at].xRot,
                                                     true, result, &error))
                {
                    Fail(source, error);
                    return true;
                }
                sources[at].x = result[0];
                sources[at].y = result[1];
                sources[at].z = result[2];
            }
            index += 4;
            continue;
        }
        if (part == L"if" || part == L"unless")
        {
            if (index + 2 >= words.size() || Lower(words[index + 1]) != L"entity")
                return false;
            const bool wanted = part == L"if";
            next.clear();
            for (size_t test = 0; test < sources.size(); ++test)
            {
                EntityList found;
                std::wstring error;
                if (!TargetSelector::Select(sources[test], words[index + 2], found, &error))
                {
                    Fail(source, error);
                    return true;
                }
                if (!found.empty() == wanted)
                    next.push_back(sources[test]);
            }
            sources.swap(next);
            tested = true;
            index += 3;
            continue;
        }
        return false;
    }
    if (!tested)
        return false;
    if (sources.empty())
        Fail(source, L"Test failed");
    else
        Tell(source, L"Test passed, count: " + Number(static_cast<int>(sources.size())));
    return true;
}
}

namespace ChatCommands
{
std::wstring CleanMessage(const std::wstring& message)
{
    std::wstring clean;
    for (size_t index = 0; index < message.size() && clean.size() < kMaxMessageLength; ++index)
    {
        const wchar_t character = message[index];
        if (character < 0x20 || (character >= 0x7f && character < 0xa0) || character == 0xa7)
            continue;
        if (character == L' ' && (clean.empty() || clean[clean.size() - 1] == L' '))
            continue;
        clean.push_back(character);
    }
    while (!clean.empty() && clean[clean.size() - 1] == L' ')
        clean.erase(clean.size() - 1);
    return clean;
}

std::wstring FormatMessage(const std::wstring& message)
{
    std::wstring formatted;
    bool visible = false;
    for (size_t index = 0; index < message.size(); ++index)
    {
        const wchar_t code = index + 1 < message.size() ? static_cast<wchar_t>(towlower(message[index + 1])) : 0;
        if (message[index] == L'&' && ((code >= L'0' && code <= L'9') || (code >= L'a' && code <= L'f')
                                       || (code >= L'k' && code <= L'o') || code == L'r'))
        {
            formatted.push_back(static_cast<wchar_t>(0xA7));
            formatted.push_back(code);
            ++index;
            continue;
        }
        if (message[index] != L' ')
            visible = true;
        formatted.push_back(message[index]);
    }
    return visible ? formatted : std::wstring();
}

void Reply(boost::shared_ptr<ServerPlayer> player, const std::wstring& text)
{
    std::wstring strings[2] = {std::wstring(), text};
    player->sendMessage(ClientboundChatPacket::eChatPacketMessage_PlayerChat, NULL, 0, strings, 2);
}

void Error(boost::shared_ptr<ServerPlayer> player, const std::wstring& text)
{
    Reply(player, L"§c" + text);
}

void Run(boost::shared_ptr<ServerPlayer> player, const std::wstring& line)
{
    const Words words = CommandArguments::Split(line.substr(1));
    if (words.empty())
        return;
    const std::wstring name = Lower(words[0]);
    const Entry* entry = FindEntry(name);
    if (entry == NULL)
    {
        Error(player, L"Unknown command /" + name + L". Type /help for the commands.");
        return;
    }
    if (!entry->everyone && !gMinecraftServer->getPlayers()->isOp(player))
    {
        Error(player, L"You don't have permission to use this command.");
        return;
    }
    CommandSource source;
    source.player = player;
    source.output = player;
    source.entity = player;
    source.level = player->mLevel;
    source.x = player->mX;
    source.y = player->mY;
    source.z = player->mZ;
    source.yRot = player->mYRot;
    source.xRot = player->mXRot;
    if (!entry->run(source, words))
        Error(player, UsageError(*entry));
}

const CommandUsage* Usages(size_t* count)
{
    static CommandUsage usages[kEntryCount];
    static bool built = false;
    if (!built)
    {
        for (size_t index = 0; index < kEntryCount; ++index)
        {
            usages[index].name = kEntries[index].name;
            usages[index].usage = kEntries[index].usage;
            usages[index].arguments = kEntries[index].arguments;
        }
        built = true;
    }
    *count = kEntryCount;
    return usages;
}
}
