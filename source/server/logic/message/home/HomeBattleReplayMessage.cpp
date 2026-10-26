#include "logic/message/home/HomeBattleReplayMessage.h"

#include "library/LibraryMessage.h"

namespace server {
namespace {
// HomeBattleReplayMessage: Int shard, then the replay id LogicLong* +0x48.
constexpr int kReplayIdOffset = 0x48;
} // namespace

void HomeBattleReplayMessage::decode() {
    const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
    replayId_ = library::readLogicLong(message.readPointer(kReplayIdOffset));
}
} // namespace server
