#include "logic/message/sector/SectorHeartbeatMessage.h"

#include "library/LibraryMessage.h"
#include "logic/command/LogicCommand.h"

namespace server {
namespace {
// SectorHeartbeatMessage: checksum +0x44, turn +0x48, LogicArrayList<LogicCommand*> inline at +0x50 (data +0x50,
// capacity +0x58, count +0x5c). The message's destructor frees the commands and the array.
constexpr int kChecksumOffset = 0x44;
constexpr int kTurnOffset = 0x48;
constexpr int kCommandDataOffset = 0x50;
constexpr int kCommandCapacityOffset = 0x58;
constexpr int kCommandCountOffset = 0x5c;
constexpr std::size_t kPointerSize = 8;
} // namespace

void SectorHeartbeatMessage::encode() {
    auto message = library::LibraryMessage::create(kMessageType);
    message.writeInt(kTurnOffset, turn_);
    message.writeInt(kChecksumOffset, checksum_);
    if (!commands_.empty()) {
        const auto data = library::allocate(kPointerSize * commands_.size());
        for (std::size_t i = 0; i < commands_.size(); ++i)
            library::writePointer(data + kPointerSize * i, LogicCommand::decode(commands_[i]).release());
        message.writePointer(kCommandDataOffset, data);
        message.writeInt(kCommandCapacityOffset, std::int32_t(commands_.size()));
        message.writeInt(kCommandCountOffset, std::int32_t(commands_.size()));
    }
    stream_.writeBytesWithoutLength(message.encode());
}
} // namespace server
