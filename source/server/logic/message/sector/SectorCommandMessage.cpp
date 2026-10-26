#include "logic/message/sector/SectorCommandMessage.h"

#include "library/LibraryMessage.h"

namespace server {
namespace {
// SectorCommandMessage: LogicCommand* +0x48 (optional), client checksum +0x50, client tick +0x54.
constexpr int kCommandOffset = 0x48;
constexpr int kClientTickOffset = 0x54;
} // namespace

void SectorCommandMessage::decode() {
    auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
    clientTick_ = message.readInt(kClientTickOffset);
    command_ = LogicCommand(message.takePointer(kCommandOffset));
}
} // namespace server
