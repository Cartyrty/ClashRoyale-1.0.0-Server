#include "logic/message/home/OutOfSyncMessage.h"

#include "library/LibraryMessage.h"

namespace server {
namespace {
// OutOfSyncMessage: server checksum +0x44, client checksum +0x48, subtick +0x4c.
constexpr int kServerChecksumOffset = 0x44;
constexpr int kClientChecksumOffset = 0x48;
constexpr int kSubtickOffset = 0x4c;
} // namespace

void OutOfSyncMessage::encode() {
    auto message = library::LibraryMessage::create(kMessageType);
    message.writeInt(kServerChecksumOffset, serverChecksum_);
    message.writeInt(kClientChecksumOffset, clientChecksum_);
    message.writeInt(kSubtickOffset, subtick_);
    stream_.writeBytesWithoutLength(message.encode());
}
} // namespace server
