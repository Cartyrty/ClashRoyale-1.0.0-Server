#pragma once
// Reading a client command's own encoding (LogicCommandManager::encodeCommand: VInt type, then the subclass's encode,
// which writes LogicCommand's base fields first or last as on the client), for the commands whose fields the server
// needs besides executing them.
#include "logic/command/LogicCommand.h"
#include "titan/logic/datastream/ByteStream.h"

namespace server {
// The command's fields after its type.
inline ByteStream readCommandFields(const LogicCommand &command) {
    ByteStream stream(command.encode());
    stream.readVInt();
    return stream;
}
} // namespace server
