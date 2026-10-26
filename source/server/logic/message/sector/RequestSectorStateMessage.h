#pragma once
// The client is out of sync in a battle and needs the full state again. Payload: VInt clientTick.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class RequestSectorStateMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 12903;

    void decode() override { clientTick_ = stream_.readVInt(); }
    int getMessageType() const override { return kMessageType; }

    int getClientTick() const { return clientTick_; }

private:
    int clientTick_ = 0;
};
} // namespace server
