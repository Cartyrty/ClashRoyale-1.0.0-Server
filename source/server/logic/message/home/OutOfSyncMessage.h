#pragma once
// The client's home went out of sync with the server's. Encoded by the library's own OutOfSyncMessage.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class OutOfSyncMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24104;

    void encode() override;
    int getMessageType() const override { return kMessageType; }

    void setServerChecksum(int checksum) { serverChecksum_ = checksum; }
    void setClientChecksum(int checksum) { clientChecksum_ = checksum; }
    void setSubtick(int tick) { subtick_ = tick; }

private:
    int serverChecksum_ = 0;
    int clientChecksum_ = 0;
    int subtick_ = 0;
};
} // namespace server
