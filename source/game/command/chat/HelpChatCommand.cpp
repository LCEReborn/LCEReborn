#include "game/command/chat/HelpChatCommand.hpp"

#include "game/command/ChatCommands.hpp"
#include "game/command/CommandArguments.hpp"

using CommandArguments::Lower;
using CommandArguments::Number;
using CommandArguments::ParseInt;

namespace
{
const size_t kHelpPageSize = 7;
}

HelpChatCommand::HelpChatCommand() : ChatCommand(L"help", L"[page|command]", L"command", ePermission_Everyone)
{
    addAlias(L"?");
}

bool HelpChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() > 2)
        return false;
    int page = 1;
    if (words.size() == 2 && !ParseInt(words[1], 1, 1000, &page))
    {
        const std::wstring name = Lower(words[1][0] == L'/' ? words[1].substr(1) : words[1]);
        ChatCommand* command = ChatCommands::Find(name);
        if (command == NULL || !command->canUse(source.permission))
        {
            Fail(source, L"Unknown command /" + name + L". Type /help for the commands.");
            return true;
        }
        Tell(source, command->usageLine());
        if (command->getName() == L"execute")
        {
            Tell(source, L"§7  as <targets>, at <targets>, positioned <x y z>, positioned as <targets>,");
            Tell(source, L"§7  if entity <targets>, unless entity <targets>, run <command>");
        }
        return true;
    }

    std::vector<ChatCommand*> listed;
    const std::vector<ChatCommand*>& commands = ChatCommands::Commands();
    for (size_t index = 0; index < commands.size(); ++index)
    {
        if (commands[index]->canUse(source.permission))
            listed.push_back(commands[index]);
    }
    const int pages = static_cast<int>((listed.size() + kHelpPageSize - 1) / kHelpPageSize);
    if (page > pages)
        page = pages;
    Tell(source, L"§2--- Showing help page " + Number(page) + L" of " + Number(pages) + L" (/help <page>) ---");
    for (size_t index = (page - 1) * kHelpPageSize; index < listed.size() && index < page * kHelpPageSize; ++index)
        Tell(source, listed[index]->usageLine());
    return true;
}
