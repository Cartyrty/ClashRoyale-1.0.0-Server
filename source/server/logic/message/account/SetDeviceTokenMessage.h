#pragma once
// The push notification token; the server does not use it.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class SetDeviceTokenMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 10113;

    int getMessageType() const override { return kMessageType; }
};
} // namespace server
