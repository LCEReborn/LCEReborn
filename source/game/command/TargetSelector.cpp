#include "game/command/TargetSelector.hpp"

#include <math.h>
#include <wctype.h>

#include <algorithm>

#include <boost/pointer_cast.hpp>

#include "game/command/CommandArguments.hpp"
#include "game/core/EntityType.hpp"
#include "game/core/GameType.hpp"
#include "game/entity/mob/Entity.hpp"
#include "game/entity/mob/EntityIO.hpp"
#include "game/server/MinecraftServer.hpp"
#include "game/server/ServerPlayer.hpp"
#include "game/util/Random.hpp"
#include "game/util/ResourceLocation.hpp"
#include "game/world/Level.hpp"
#include "game/world/generation/PlayerList.hpp"

namespace
{
struct Filter
{
    bool hasType;
    bool typeNegated;
    std::wstring type;
    bool hasName;
    bool nameNegated;
    std::wstring name;
    bool hasMode;
    bool modeNegated;
    int mode;
    int minimumLevel;
    int maximumLevel;
    double minimumRadius;
    double maximumRadius;
    int count;
    bool hasCount;
    double x;
    double y;
    double z;
    bool positioned;
    bool hasVolume;
    double dx;
    double dy;
    double dz;
};

struct Ranked
{
    boost::shared_ptr<Entity> entity;
    double distance;
};

bool Nearer(const Ranked& left, const Ranked& right)
{
    return left.distance < right.distance;
}

bool Farther(const Ranked& left, const Ranked& right)
{
    return left.distance > right.distance;
}

bool ParseMode(const std::wstring& value, int* mode)
{
    const std::wstring lower = CommandArguments::Lower(value);
    if (lower == L"s" || lower == L"survival" || lower == L"0")
        *mode = 0;
    else if (lower == L"c" || lower == L"creative" || lower == L"1")
        *mode = 1;
    else if (lower == L"a" || lower == L"adventure" || lower == L"2")
        *mode = 2;
    else if (lower == L"sp" || lower == L"spectator" || lower == L"3")
        *mode = 3;
    else
        return false;
    return true;
}

bool ParseArguments(const std::wstring& text, const CommandSource& source, Filter* filter,
                    std::wstring* error)
{
    size_t start = 0;
    while (start < text.size())
    {
        size_t end = text.find(L',', start);
        if (end == std::wstring::npos)
            end = text.size();
        const std::wstring pair = text.substr(start, end - start);
        start = end + 1;
        if (pair.empty())
            continue;
        const size_t equals = pair.find(L'=');
        if (equals == std::wstring::npos)
        {
            *error = L"Selector arguments are written as key=value.";
            return false;
        }
        const std::wstring key = CommandArguments::Lower(pair.substr(0, equals));
        std::wstring value = pair.substr(equals + 1);
        const bool negated = !value.empty() && value[0] == L'!';
        if (negated)
            value = value.substr(1);
        double number = 0.0;
        int integer = 0;
        if (key == L"type")
        {
            filter->hasType = true;
            filter->typeNegated = negated;
            filter->type = CommandArguments::StripNamespace(value);
            if (filter->type == L"lightningbolt")
                filter->type = L"lightning_bolt";
            if (filter->type != L"player" && filter->type != L"lightning_bolt"
                && !EntityIO::isValidEntityType(ResourceLocation(filter->type)))
            {
                *error = L"Unknown entity type " + value + L".";
                return false;
            }
        }
        else if (key == L"name")
        {
            filter->hasName = true;
            filter->nameNegated = negated;
            filter->name = CommandArguments::Lower(value);
        }
        else if (key == L"m")
        {
            if (!ParseMode(value, &filter->mode))
            {
                *error = L"Unknown game mode " + value + L".";
                return false;
            }
            filter->hasMode = true;
            filter->modeNegated = negated;
        }
        else if ((key == L"l" || key == L"lm") && !negated && CommandArguments::ParseInt(value, 0, 1000000, &integer))
        {
            if (key == L"l")
                filter->maximumLevel = integer;
            else
                filter->minimumLevel = integer;
        }
        else if ((key == L"r" || key == L"rm") && !negated && CommandArguments::ParseNumber(value, &number)
                 && number >= 0.0)
        {
            if (key == L"r")
                filter->maximumRadius = number;
            else
                filter->minimumRadius = number;
            filter->positioned = true;
        }
        else if (key == L"c" && !negated && CommandArguments::ParseInt(value, -1000000, 1000000, &integer)
                 && integer != 0)
        {
            filter->count = integer;
            filter->hasCount = true;
        }
        else if ((key == L"x" || key == L"y" || key == L"z") && !negated)
        {
            const double origin = key == L"x" ? source.x : (key == L"y" ? source.y : source.z);
            if (!CommandArguments::ParseCoordinate(value, origin, false, &number))
            {
                *error = L"Selector coordinate " + key + L" must be a number.";
                return false;
            }
            (key == L"x" ? filter->x : (key == L"y" ? filter->y : filter->z)) = number;
            filter->positioned = true;
        }
        else if ((key == L"dx" || key == L"dy" || key == L"dz") && !negated
                 && CommandArguments::ParseNumber(value, &number))
        {
            (key == L"dx" ? filter->dx : (key == L"dy" ? filter->dy : filter->dz)) = number;
            filter->hasVolume = true;
            filter->positioned = true;
        }
        else
        {
            *error = L"Unknown or invalid selector argument " + pair + L".";
            return false;
        }
    }
    return true;
}

bool Matches(const boost::shared_ptr<Entity>& entity, const Filter& filter)
{
    if (!entity->isAlive())
        return false;
    if (filter.hasType && (TargetSelector::TypeName(entity) == filter.type) == filter.typeNegated)
        return false;
    if (filter.hasName && (CommandArguments::Lower(entity->getName()) == filter.name) == filter.nameNegated)
        return false;

    boost::shared_ptr<ServerPlayer> player = boost::dynamic_pointer_cast<ServerPlayer>(entity);
    if (filter.hasMode || filter.minimumLevel > 0 || filter.maximumLevel >= 0)
    {
        if (!player)
            return false;
        if (filter.hasMode && (player->GetGameType()->getId() == filter.mode) == filter.modeNegated)
            return false;
        if (player->mExperienceLevel < filter.minimumLevel)
            return false;
        if (filter.maximumLevel >= 0 && player->mExperienceLevel > filter.maximumLevel)
            return false;
    }

    const double distance = sqrt(entity->distanceToSqr(filter.x, filter.y, filter.z));
    if (filter.maximumRadius >= 0.0 && distance > filter.maximumRadius)
        return false;
    if (distance < filter.minimumRadius)
        return false;
    if (filter.hasVolume)
    {
        const double x0 = filter.dx < 0.0 ? filter.x + filter.dx : filter.x;
        const double y0 = filter.dy < 0.0 ? filter.y + filter.dy : filter.y;
        const double z0 = filter.dz < 0.0 ? filter.z + filter.dz : filter.z;
        if (entity->mX < floor(x0) || entity->mX >= floor(x0) + fabs(filter.dx) + 1.0 || entity->mY < floor(y0)
            || entity->mY >= floor(y0) + fabs(filter.dy) + 1.0 || entity->mZ < floor(z0)
            || entity->mZ >= floor(z0) + fabs(filter.dz) + 1.0)
            return false;
    }
    return true;
}
}

namespace TargetSelector
{
bool IsSelector(const std::wstring& word)
{
    return word.size() >= 2 && word[0] == L'@';
}

std::wstring TypeName(const boost::shared_ptr<Entity>& entity)
{
    if (Entity::instanceof(entity, EntityType::PLAYER))
        return L"player";
    const std::wstring path = CommandArguments::Lower(EntityIO::getKey(entity).getPath());
    return path == L"lightningbolt" ? std::wstring(L"lightning_bolt") : path;
}

std::wstring DisplayName(const boost::shared_ptr<Entity>& entity)
{
    const std::wstring name = entity->getName();
    return name.empty() ? CommandArguments::Pretty(TypeName(entity)) : name;
}

std::wstring Names(const std::vector<boost::shared_ptr<Entity> >& entities)
{
    std::wstring text;
    for (size_t index = 0; index < entities.size(); ++index)
    {
        if (index > 0)
            text += index + 1 == entities.size() ? L" and " : L", ";
        text += DisplayName(entities[index]);
    }
    return text;
}

std::wstring Names(const std::vector<boost::shared_ptr<ServerPlayer> >& players)
{
    std::vector<boost::shared_ptr<Entity> > entities(players.begin(), players.end());
    return Names(entities);
}

bool Select(const CommandSource& source, const std::wstring& word, std::vector<boost::shared_ptr<Entity> >& entities,
            std::wstring* error)
{
    entities.clear();
    if (!IsSelector(word))
    {
        boost::shared_ptr<ServerPlayer> player = gMinecraftServer->getPlayers()->getPlayerByNameIgnoringCase(word);
        if (player)
            entities.push_back(player);
        return true;
    }

    const wchar_t kind = static_cast<wchar_t>(towlower(word[1]));
    if (kind != L'p' && kind != L'a' && kind != L'r' && kind != L's' && kind != L'e')
    {
        *error = L"Unknown selector " + word.substr(0, 2) + L". Use @p, @a, @r, @s or @e.";
        return false;
    }

    Filter filter;
    filter.hasType = false;
    filter.typeNegated = false;
    filter.hasName = false;
    filter.nameNegated = false;
    filter.hasMode = false;
    filter.modeNegated = false;
    filter.mode = 0;
    filter.minimumLevel = 0;
    filter.maximumLevel = -1;
    filter.minimumRadius = 0.0;
    filter.maximumRadius = -1.0;
    filter.count = 0;
    filter.hasCount = false;
    filter.x = source.x;
    filter.y = source.y;
    filter.z = source.z;
    filter.positioned = false;
    filter.hasVolume = false;
    filter.dx = 0.0;
    filter.dy = 0.0;
    filter.dz = 0.0;
    if (word.size() > 2)
    {
        if (word[2] != L'[' || word[word.size() - 1] != L']')
        {
            *error = L"Selector arguments go in square brackets, like @e[type=zombie].";
            return false;
        }
        if (!ParseArguments(word.substr(3, word.size() - 4), source, &filter, error))
            return false;
    }

    const bool playersOnly = kind == L'p' || kind == L'a'
                             || (kind == L'r' && (!filter.hasType || filter.type == L"player"));
    std::vector<boost::shared_ptr<Entity> > candidates;
    if (kind == L's')
    {
        if (source.entity)
            candidates.push_back(source.entity);
    }
    else if (playersOnly)
    {
        std::vector<boost::shared_ptr<ServerPlayer> >* players = gMinecraftServer->getPlayers()->getPlayers();
        for (size_t index = 0; index < players->size(); ++index)
        {
            const boost::shared_ptr<ServerPlayer>& player = (*players)[index];
            if ((kind == L'p' || filter.positioned) && player->mLevel != source.level)
                continue;
            candidates.push_back(player);
        }
    }
    else
    {
        candidates = source.level->getAllEntities();
    }

    std::vector<Ranked> ranked;
    for (size_t index = 0; index < candidates.size(); ++index)
    {
        if (!Matches(candidates[index], filter))
            continue;
        Ranked entry;
        entry.entity = candidates[index];
        entry.distance = candidates[index]->distanceToSqr(filter.x, filter.y, filter.z);
        ranked.push_back(entry);
    }

    int limit = filter.hasCount ? filter.count : 0;
    if (kind == L'r')
    {
        Random* random = source.level->mRandom;
        for (size_t index = ranked.size(); index > 1; --index)
        {
            const size_t other = static_cast<size_t>(random->nextInt(static_cast<int>(index)));
            std::swap(ranked[index - 1], ranked[other]);
        }
        if (!filter.hasCount)
            limit = 1;
    }
    else if (kind == L'p' || filter.hasCount)
    {
        std::stable_sort(ranked.begin(), ranked.end(), limit < 0 ? Farther : Nearer);
        if (kind == L'p' && !filter.hasCount)
            limit = 1;
    }
    const size_t wanted = limit == 0 ? ranked.size() : static_cast<size_t>(limit < 0 ? -limit : limit);
    for (size_t index = 0; index < ranked.size() && index < wanted; ++index)
        entities.push_back(ranked[index].entity);

    return true;
}

bool Entities(const CommandSource& source, const std::wstring& word,
              std::vector<boost::shared_ptr<Entity> >& entities, std::wstring* error)
{
    if (!Select(source, word, entities, error))
        return false;
    if (!entities.empty())
        return true;
    if (!IsSelector(word))
        *error = L"There is no player called " + word + L".";
    else
    {
        const wchar_t kind = static_cast<wchar_t>(towlower(word[1]));
        const bool typed = word.find(L"type=") != std::wstring::npos && word.find(L"type=player") == std::wstring::npos;
        *error = kind == L'e' || (kind == L'r' && typed) ? L"No entity was found." : L"No player was found.";
    }
    return false;
}

bool Players(const CommandSource& source, const std::wstring& word,
             std::vector<boost::shared_ptr<ServerPlayer> >& players, std::wstring* error)
{
    players.clear();
    std::vector<boost::shared_ptr<Entity> > entities;
    if (!Entities(source, word, entities, error))
        return false;
    for (size_t index = 0; index < entities.size(); ++index)
    {
        boost::shared_ptr<ServerPlayer> player = boost::dynamic_pointer_cast<ServerPlayer>(entities[index]);
        if (player)
            players.push_back(player);
    }
    if (players.empty())
    {
        *error = L"No player was found.";
        return false;
    }
    return true;
}
}
