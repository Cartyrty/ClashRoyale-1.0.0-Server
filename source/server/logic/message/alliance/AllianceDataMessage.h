#pragma once
// A clan's page.
#include "logic/message/alliance/AllianceFullEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceDataMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24301;

    void encode() override { alliance_.encode(stream_); }
    int getMessageType() const override { return kMessageType; }

    void setAllianceFullEntry(AllianceFullEntry alliance) { alliance_ = std::move(alliance); }

private:
    AllianceFullEntry alliance_;
};
} // namespace server
