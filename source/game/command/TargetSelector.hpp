#ifndef GAME_COMMAND_TARGETSELECTOR_HPP
#define GAME_COMMAND_TARGETSELECTOR_HPP

#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "game/command/CommandSource.hpp"

class Entity;
class ServerPlayer;

namespace TargetSelector
{
bool IsSelector(const std::wstring& word);
bool Select(const CommandSource& source, const std::wstring& word, std::vector<boost::shared_ptr<Entity> >& entities,
            std::wstring* error);
bool Entities(const CommandSource& source, const std::wstring& word,
              std::vector<boost::shared_ptr<Entity> >& entities, std::wstring* error);
bool Players(const CommandSource& source, const std::wstring& word,
             std::vector<boost::shared_ptr<ServerPlayer> >& players, std::wstring* error);
std::wstring TypeName(const boost::shared_ptr<Entity>& entity);
std::wstring DisplayName(const boost::shared_ptr<Entity>& entity);
std::wstring Names(const std::vector<boost::shared_ptr<Entity> >& entities);
std::wstring Names(const std::vector<boost::shared_ptr<ServerPlayer> >& players);
}

#endif
