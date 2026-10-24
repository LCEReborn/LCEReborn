#ifndef GAME_COMMAND_CHAT_WEATHERCHATCOMMAND_HPP
#define GAME_COMMAND_CHAT_WEATHERCHATCOMMAND_HPP

#include "game/command/ChatCommand.hpp"

class WeatherChatCommand : public ChatCommand
{
public:
    WeatherChatCommand();

    virtual bool run(const CommandSource& source, const Words& words);
};

#endif
