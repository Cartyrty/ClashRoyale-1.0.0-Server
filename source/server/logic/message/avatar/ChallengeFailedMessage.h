#pragma once
// A friendly battle could not start. Encoded by the library's own ChallengeFailedMessage, which has no fields: the
// client gets no reason.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class ChallengeFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24121;

    void encode() override { stream_.writeBytesWithoutLength(library::LibraryMessage::create(kMessageType).encode()); }
    int getMessageType() const override { return kMessageType; }
};
} // namespace server
