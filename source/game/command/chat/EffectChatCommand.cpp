#include "game/command/chat/EffectChatCommand.hpp"

#include "game/command/CommandArguments.hpp"
#include "game/command/TargetSelector.hpp"
#include "game/entity/mob/Entity.hpp"
#include "game/entity/mob/LivingEntity.hpp"
#include "game/entity/mob/MobEffect.hpp"
#include "game/entity/mob/MobEffectInstance.hpp"

using CommandArguments::Lower;
using CommandArguments::Number;
using CommandArguments::ParseInt;

EffectChatCommand::EffectChatCommand()
    : ChatCommand(L"effect", L"<targets> <effect|clear> [seconds] [amplifier] [hideParticles]",
                  L"targets effect number number true|false", ePermission_Operator)
{
}

bool EffectChatCommand::run(const CommandSource& source, const Words& words)
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
                    Fail(source,
                         L"Couldn't take " + name + L" from " + targetName + L" as they do not have the effect");
                continue;
            }
            target->removeEffect(effect);
            ++given;
            if (targets.size() == 1)
                Tell(source, L"Took " + name + L" from " + targetName);
            continue;
        }
        MobEffectInstance* instance = new MobEffectInstance(effect, instant ? duration : duration * 20, amplifier,
                                                            false, !hideParticles);
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
