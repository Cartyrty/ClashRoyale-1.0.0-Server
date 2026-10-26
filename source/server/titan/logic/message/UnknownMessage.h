#pragma once
// A message of a type the server does not handle: kept as it came, for the log.
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class UnknownMessage : public PiranhaMessage {
public:
    explicit UnknownMessage(int messageType) : messageType_(messageType) {}

    int getMessageType() const override { return messageType_; }

private:
    int messageType_;
};
} // namespace server
