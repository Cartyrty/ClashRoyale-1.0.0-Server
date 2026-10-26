#pragma once
// A command the server gives a client (a LogicServerCommand subclass). It is built from its encoding, as
// LogicCommandManager::encodeCommand writes it: VInt type, the command's own fields, then LogicServerCommand::encode's
// VInt id and LogicCommand::encode's VInt tick when given and VInt execute tick (both -1: GameMode::addCommand
// assigns them on the client) and LogicLong executor (0: the receiver).
#include "logic/command/LogicCommand.h"
#include "titan/logic/datastream/ByteStream.h"

namespace server {
class LogicServerCommand {
public:
    virtual ~LogicServerCommand() = default;

    virtual int getCommandType() const = 0;
    // The library's command (LogicCommandManager::decodeCommand of the encoding); empty if it does not decode.
    LogicCommand createCommand(int serverCommandId) const;

protected:
    virtual void encodeFields(ByteStream &stream) const = 0;
};
} // namespace server
