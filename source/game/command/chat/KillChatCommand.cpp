#include "game/command/chat/KillChatCommand.hpp"

#include "game/command/TargetSelector.hpp"
#include "game/entity/mob/Entity.hpp"

KillChatCommand::KillChatCommand() : ChatCommand(L"kill", L"[targets]", L"targets", ePermission_Operator)
{
}

bool KillChatCommand::run(const CommandSource& source, const Words& words)
{
    if (words.size() > 2)
        return false;
    EntityList targets;
    if (words.size() == 2)
    {
        if (!ResolveEntities(source, words[1], targets))
            return true;
    }
    else if (source.entity)
        targets.push_back(source.entity);
    else
    {
        Fail(source, L"No entity was found.");
        return true;
    }
    for (size_t index = 0; index < targets.size(); ++index)
        targets[index]->kill();
    if (targets.size() == 1)
        Tell(source, L"Killed " + TargetSelector::DisplayName(targets[0]));
    else
        Tell(source, L"Killed " + Count(static_cast<int>(targets.size()), L"entity", L"entities"));
    return true;
}
