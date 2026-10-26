#include "logic/message/home/OwnHomeDataMessage.h"

#include "library/LibraryMessage.h"
#include "logic/avatar/LogicClientAvatar.h"
#include "logic/home/LogicClientHome.h"

namespace server {
namespace {
// OwnHomeDataMessage: LogicClientHome* +0x48, LogicClientAvatar* +0x50 (both owned by the message), seed +0x58.
constexpr int kHomeOffset = 0x48;
constexpr int kAvatarOffset = 0x50;
constexpr int kRandomSeedOffset = 0x58;
} // namespace

void OwnHomeDataMessage::encode() {
    auto message = library::LibraryMessage::create(kMessageType);
    message.writePointer(kHomeOffset, LogicClientHome::decode(home_).release());
    message.writePointer(kAvatarOffset, LogicClientAvatar::decode(avatar_).release());
    message.writeInt(kRandomSeedOffset, randomSeed_);
    stream_.writeBytesWithoutLength(message.encode());
}

bool OwnHomeDataMessage::decodeHomeAndAvatar(const std::vector<std::uint8_t> &payload, std::vector<std::uint8_t> &home,
                                             std::vector<std::uint8_t> &avatar) {
    const auto message = library::LibraryMessage::decode(kMessageType, payload);
    const auto decodedHome = message.readPointer(kHomeOffset);
    const auto decodedAvatar = message.readPointer(kAvatarOffset);
    if (!decodedHome || !decodedAvatar) return false;
    home = LogicClientHome::view(decodedHome).encode();
    avatar = LogicClientAvatar::view(decodedAvatar).encode();
    return true;
}
} // namespace server
