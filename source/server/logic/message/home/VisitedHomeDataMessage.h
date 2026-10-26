#pragma once
// Another player's profile: their home and avatar. One stream, as VisitedHomeDataMessage::encode writes it (the
// Boolean packing continues across the parts): VInt, LogicClientHome, owner LogicClientAvatar, Boolean visitor avatar
// present.
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
        stream_.writeVInt(0);
        LogicClientHome::decode(home_).encode(stream_);
        LogicClientAvatar::decode(avatar_).encode(stream_);
        stream_.writeBoolean(false);
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
