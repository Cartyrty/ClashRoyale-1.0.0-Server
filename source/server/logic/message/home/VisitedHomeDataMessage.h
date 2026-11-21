#pragma once
// Another player's profile: their home and avatar. One stream, as VisitedHomeDataMessage::encode writes it (the
// Boolean packing continues across the parts): VInt, LogicClientHome, owner LogicClientAvatar, Boolean visitor avatar
// present. Encoded by the library's own VisitedHomeDataMessage.
#include "library/LibraryMessage.h"
#include "logic/avatar/LogicClientAvatar.h"
#include "logic/home/LogicClientHome.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <vector>

namespace server {
class VisitedHomeDataMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24113;

    void encode() override {
        // VisitedHomeDataMessage: VInt +0x44, LogicClientHome* +0x48, owner LogicClientAvatar* +0x50, visitor
        // LogicClientAvatar* +0x58 (null: none). The message frees them.
        constexpr int kHomeOffset = 0x48;
        constexpr int kOwnerAvatarOffset = 0x50;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writePointer(kHomeOffset, LogicClientHome::decode(home_).release());
        message.writePointer(kOwnerAvatarOffset, LogicClientAvatar::decode(avatar_).release());
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }
    int getMessageVersion() const override { return 10; }

    void setHome(const std::vector<std::uint8_t> &encodedHome) { home_ = encodedHome; }
    void setOwnerAvatar(const std::vector<std::uint8_t> &encodedAvatar) { avatar_ = encodedAvatar; }

private:
    std::vector<std::uint8_t> home_;
    std::vector<std::uint8_t> avatar_;
};
} // namespace server
