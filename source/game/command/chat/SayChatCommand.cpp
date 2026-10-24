#include "game/command/chat/SayChatCommand.hpp"

#include "game/command/ChatCommands.hpp"
#include "game/command/CommandArguments.hpp"

SayChatCommand::SayChatCommand() : ChatCommand(L"say", L"<message>", L"text", ePermission_Operator)
{
}

bool SayChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() < 2)
        return false;
    const std::wstring message = ChatCommands::FormatMessage(CommandArguments::Join(words, 1));
    if (message.empty())
        return false;
    Broadcast(L"[" + SourceName(source) + L"] " + message);
    return true;
}
