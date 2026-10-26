#pragma once
// A clan's page. Payload: Long allianceId.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class AskForAllianceDataMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14302;

    void decode() override { allianceId_ = stream_.readLong(); }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getAllianceId() const { return allianceId_; }

private:
    std::int64_t allianceId_ = 0;
};
} // namespace server
