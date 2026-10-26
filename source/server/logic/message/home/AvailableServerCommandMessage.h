#pragma once
// A server command for the client to execute; the client echoes it back in its next EndClientTurnMessage. Encoded by
// the library's own AvailableServerCommandMessage.
#include "logic/command/LogicCommand.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AvailableServerCommandMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24111;

    void encode() override;
    int getMessageType() const override { return kMessageType; }

    void setServerCommand(LogicCommand command) { command_ = std::move(command); }

private:
    LogicCommand command_;
};
} // namespace server
