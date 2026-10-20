#ifndef GAME_COMMAND_COMMANDSOURCE_HPP
#define GAME_COMMAND_COMMANDSOURCE_HPP

#include <boost/shared_ptr.hpp>

class Entity;
class Level;
class ServerPlayer;

struct CommandSource
{
    boost::shared_ptr<ServerPlayer> player;
    boost::shared_ptr<ServerPlayer> output;
    boost::shared_ptr<Entity> entity;
    Level* level;
    double x;
    double y;
    double z;
    float yRot;
    float xRot;
};

#endif
