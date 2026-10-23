#include "game/command/chat/MeChatCommand.hpp"

#include "game/command/ChatCommands.hpp"
#include "game/command/CommandArguments.hpp"

MeChatCommand::MeChatCommand() : ChatCommand(L"me", L"<action>", L"text", ePermission_Everyone)
{
}

bool MeChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() < 2)
        return false;
    const std::wstring message = ChatCommands::FormatMessage(CommandArguments::Join(words, 1));
    if (message.empty())
        return false;
    Broadcast(L"* " + SourceName(source) + L" " + message);
    return true;
}
