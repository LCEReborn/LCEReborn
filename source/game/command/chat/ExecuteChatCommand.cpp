#include "game/command/chat/ExecuteChatCommand.hpp"

#include <boost/pointer_cast.hpp>

#include "game/command/ChatCommands.hpp"
#include "game/command/CommandArguments.hpp"
#include "game/command/TargetSelector.hpp"
#include "game/entity/mob/Entity.hpp"
#include "game/server/ServerPlayer.hpp"

using CommandArguments::Lower;
using CommandArguments::Number;

namespace
{
const size_t kExecuteLimit = 4096;
}

ExecuteChatCommand::ExecuteChatCommand()
    : ChatCommand(L"execute", L"<as|at|positioned|if|unless|run> ...", L"execute", ePermission_Operator)
{
}

bool ExecuteChatCommand::run(const CommandSource& source, const Words& words)
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
            ChatCommand* command = ChatCommands::Find(name);
            if (command == NULL)
            {
                Fail(source, L"Unknown command /" + name + L". Type /help for the commands.");
                return true;
            }
            if (!command->canUse(source.permission))
            {
                Fail(source, L"You don't have permission to use this command.");
                return true;
            }
            const Words rest(words.begin() + index + 1, words.end());
            for (size_t run = 0; run < sources.size(); ++run)
            {
                if (!command->run(sources[run], rest))
                {
                    Fail(source, command->usageError());
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
                if (!CommandArguments::ParsePosition(words, index + 1, origin, sources[at].yRot, sources[at].xRot, true,
                                                     result, &error))
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

bool ExecuteChatCommand::Fork(const CommandSource& source, const std::vector<CommandSource>& from,
                              const std::wstring& word, bool moveTo, bool rotate, bool becomeEntity,
                              std::vector<CommandSource>& to)
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
                Fail(source,
                     L"That would run the command more than " + Number(static_cast<int>(kExecuteLimit)) + L" times.");
                return false;
            }
        }
    }
    return true;
}
