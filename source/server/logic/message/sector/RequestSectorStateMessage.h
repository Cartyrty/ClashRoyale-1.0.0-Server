#pragma once
// The client is out of sync in a battle and needs the full state again. Decoded by the library's own
// RequestSectorStateMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class RequestSectorStateMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 12903;

    void decode() override {
        // RequestSectorStateMessage: client tick +0x44.
        constexpr int kClientTickOffset = 0x44;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        clientTick_ = message.readInt(kClientTickOffset);
    }
    int getMessageType() const override { return kMessageType; }

    int getClientTick() const { return clientTick_; }

private:
    int clientTick_ = 0;
};
} // namespace server
