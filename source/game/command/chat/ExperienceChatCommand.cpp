#include "game/command/chat/ExperienceChatCommand.hpp"

#include <wctype.h>
#include <algorithm>

#include "game/command/CommandArguments.hpp"
#include "game/server/ServerPlayer.hpp"

using CommandArguments::Lower;
using CommandArguments::Number;
using CommandArguments::ParseInt;

namespace
{
void AddPoints(const boost::shared_ptr<ServerPlayer>& player, int amount)
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

int Points(const boost::shared_ptr<ServerPlayer>& player)
{
    return static_cast<int>(player->mExperienceProgress * player->getXpNeededForNextLevel());
}

bool ParseExperienceUnit(const ChatCommand::Words& words, size_t index, bool* levels)
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
}

ExperienceChatCommand::ExperienceChatCommand()
    : ChatCommand(L"xp", L"<add|set|query> <targets> <amount> [points|levels]",
                  L"add|set|query targets xpvalue points|levels", ePermission_Operator)
{
    addAlias(L"experience");
}

bool ExperienceChatCommand::run(const CommandSource& source, const Words& words)
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
                Tell(source, target->getName() + L" has "
                                 + Count(target->mExperienceLevel, L"experience level", L"experience levels"));
            else
                Tell(source,
                     target->getName() + L" has " + Count(Points(target), L"experience point", L"experience points"));
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
            Fail(source,
                 set ? L"The amount must be a whole number of 0 or more." : L"The amount must be a whole number.");
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
        Tell(source,
             (amount < 0 ? L"Took " + Number(-amount) + unit + L" from " : L"Gave " + Number(amount) + unit + L" to ")
                 + target->getName());
    }
    return true;
}
