#pragma once
// The opponent is back in the battle. Encoded by the library's own message.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class OpponentRejoinsMatchNotificationMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 20802;

    void encode() override { stream_.writeBytesWithoutLength(library::LibraryMessage::create(kMessageType).encode()); }
    int getMessageType() const override { return kMessageType; }
};
} // namespace server
