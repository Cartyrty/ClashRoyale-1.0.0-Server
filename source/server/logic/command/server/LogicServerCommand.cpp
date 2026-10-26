#include "logic/command/server/LogicServerCommand.h"

#include "titan/logic/datastream/ByteStreamHelper.h"

namespace server {
namespace {
constexpr int kAssignedByClient = -1;
constexpr std::int64_t kReceiver = 0;
} // namespace

LogicCommand LogicServerCommand::createCommand(int serverCommandId) const {
    ByteStream stream;
    stream.writeVInt(getCommandType());
    encodeFields(stream);
    stream.writeVInt(serverCommandId);
    stream.writeVInt(kAssignedByClient);
    stream.writeVInt(kAssignedByClient);
    ByteStreamHelper::encodeLogicLong(stream, kReceiver);
    return LogicCommand::decode(stream.getByteArray());
}
} // namespace server
