#pragma once
// A PvP player's command (a card placement), or only a report of the client's tick. Decoded by the library's own
// SectorCommandMessage; the message hands its command over.
#include "logic/command/LogicCommand.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class SectorCommandMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 12904;

    void decode() override;
    int getMessageType() const override { return kMessageType; }

    int getClientTick() const { return clientTick_; }
    LogicCommand &getCommand() { return command_; } // empty for a tick report

private:
    int clientTick_ = 0;
    LogicCommand command_;
};
} // namespace server
