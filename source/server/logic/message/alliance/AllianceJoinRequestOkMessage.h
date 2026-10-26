#pragma once
// The join request was sent.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceJoinRequestOkMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24319;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
