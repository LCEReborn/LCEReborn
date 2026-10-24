#include "game/command/chat/GameModeChatCommand.hpp"

#include "game/command/GameModeCommand.hpp"
#include "game/core/GameType.hpp"
#include "game/server/ServerPlayer.hpp"

GameModeChatCommand::GameModeChatCommand()
    : ChatCommand(L"gamemode", L"<mode> [player]", L"mode targets", ePermission_Operator)
{
}

bool GameModeChatCommand::run(const CommandSource& source, const Words& words)
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
