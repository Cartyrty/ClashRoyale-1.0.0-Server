#pragma once
// The client's turn: the commands it executed (each with its execute tick), its tick at the end of the turn and its
// checksum there. Sent from the home and from NPC and PvP battles. Decoded by the library's own EndClientTurnMessage;
// the message hands its commands over.
#include "logic/command/LogicCommand.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class EndClientTurnMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14102;

    void decode() override;
    int getMessageType() const override { return kMessageType; }

    int getTick() const { return tick_; }
    int getChecksum() const { return checksum_; }
    std::vector<LogicCommand> &getCommands() { return commands_; }

private:
    int tick_ = 0;
    int checksum_ = 0;
    std::vector<LogicCommand> commands_;
};
} // namespace server
