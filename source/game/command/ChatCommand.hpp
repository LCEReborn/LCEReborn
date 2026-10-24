#ifndef GAME_COMMAND_CHATCOMMAND_HPP
#define GAME_COMMAND_CHATCOMMAND_HPP

#include <stddef.h>

#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "game/command/CommandSource.hpp"

class Block;
class BlockPos;
class Enchantment;
class Entity;
class GameCommandPacket;
class GameType;
class Item;
class LivingEntity;
class MobEffect;
class ServerPlayer;

class ChatCommand
{
public:
    enum EPermission
    {
        ePermission_Everyone = 0,
        ePermission_Operator = 2,
        ePermission_Console = 4
    };

    typedef std::vector<std::wstring> Words;

    ChatCommand(const std::wstring& name, const std::wstring& usage, const std::wstring& arguments, int permission);
    virtual ~ChatCommand();

    virtual bool run(const CommandSource& source, const Words& words) = 0;

    const std::wstring& getName() const;
    const std::wstring& getUsage() const;
    const std::wstring& getArguments() const;
    int getPermission() const;
    const std::vector<std::wstring>& getAliases() const;
    bool canUse(int permission) const;
    std::wstring usageLine() const;
    std::wstring usageError() const;

protected:
    typedef boost::shared_ptr<ServerPlayer> PlayerPtr;
    typedef boost::shared_ptr<Entity> EntityPtr;
    typedef boost::shared_ptr<LivingEntity> LivingPtr;
    typedef std::vector<PlayerPtr> PlayerVector;
    typedef std::vector<EntityPtr> EntityList;
    typedef std::vector<LivingPtr> LivingList;

    void addAlias(const std::wstring& alias);

    static std::wstring LongNumber(long long value);
    static std::wstring Position(double x, double y, double z);
    static std::wstring Position(const BlockPos& pos);
    static std::wstring Count(int count, const wchar_t* one, const wchar_t* many);
    static void Tell(const CommandSource& source, const std::wstring& text);
    static void Fail(const CommandSource& source, const std::wstring& text);
    static std::wstring SourceName(const CommandSource& source);
    static BlockPos SourceBlockPos(const CommandSource& source);
    static bool InOverworld(const CommandSource& source);
    static void Perform(const CommandSource& source, boost::shared_ptr<GameCommandPacket> packet);
    static void Broadcast(const std::wstring& text);
    static bool ResolvePlayers(const CommandSource& source, const Words& words, size_t index, PlayerVector& players);
    static bool ResolveEntities(const CommandSource& source, const std::wstring& word, EntityList& entities);
    static bool ResolveLiving(const CommandSource& source, const Words& words, size_t index, LivingList& living);
    static bool ParseGameType(const std::wstring& word, const GameType** type);
    static Item* FindItem(const CommandSource& source, const std::wstring& word);
    static Block* FindBlock(const CommandSource& source, const std::wstring& word);
    static MobEffect* FindEffect(const CommandSource& source, const std::wstring& word);
    static std::wstring EffectName(MobEffect* effect);
    static Enchantment* FindEnchantment(const CommandSource& source, const std::wstring& word);
    static std::wstring EnchantmentName(Enchantment* enchantment);
    static bool ParsePosition(const CommandSource& source, const Words& words, size_t index, bool centre,
                              double result[3]);
    static bool ParseBlockPos(const CommandSource& source, const Words& words, size_t index, BlockPos* pos);
    static bool ParseBlockData(const CommandSource& source, const Words& words, size_t index, int* data);
    static bool ParseFlag(const std::wstring& word, bool* value);

private:
    std::wstring mName;
    std::wstring mUsage;
    std::wstring mArguments;
    int mPermission;
    std::vector<std::wstring> mAliases;
};

#endif
