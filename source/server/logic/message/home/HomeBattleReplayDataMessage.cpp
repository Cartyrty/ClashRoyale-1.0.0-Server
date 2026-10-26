#include "logic/message/home/HomeBattleReplayDataMessage.h"

#include "library/LibraryMessage.h"

namespace server {
namespace {
// HomeBattleReplayDataMessage: replay bytes +0x48 (owned by the message), length +0x50.
constexpr int kReplayDataOffset = 0x48;
constexpr int kReplayLengthOffset = 0x50;
} // namespace

void HomeBattleReplayDataMessage::encode() {
    auto message = library::LibraryMessage::create(kMessageType);
    message.writePointer(kReplayDataOffset, library::copyBytes(replayData_));
    message.writeInt(kReplayLengthOffset, std::int32_t(replayData_.size()));
    stream_.writeBytesWithoutLength(message.encode());
}
} // namespace server
