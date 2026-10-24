#include "game/command/chat/MessageChatCommand.hpp"

#include "game/command/ChatCommands.hpp"
#include "game/command/CommandArguments.hpp"
#include "game/server/ServerPlayer.hpp"

MessageChatCommand::MessageChatCommand()
    : ChatCommand(L"msg", L"<player> <message>", L"targets text", ePermission_Everyone)
{
    addAlias(L"tell");
    addAlias(L"w");
}

bool MessageChatCommand::run(const CommandSource& source, const Words& words)
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
