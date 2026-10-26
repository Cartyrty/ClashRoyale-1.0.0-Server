#pragma once
// The opponent's connection dropped; the battle waits for them. Encoded by the library's own message.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class OpponentLeftMatchNotificationMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 20801;

    void encode() override { stream_.writeBytesWithoutLength(library::LibraryMessage::create(kMessageType).encode()); }
    int getMessageType() const override { return kMessageType; }
};
} // namespace server
