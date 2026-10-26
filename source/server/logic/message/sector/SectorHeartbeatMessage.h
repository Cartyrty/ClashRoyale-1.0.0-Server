#pragma once
// One PvP turn (10 ticks): its number, the checksum at its end, and the commands it carries. Encoded by the library's
// own SectorHeartbeatMessage.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <vector>

namespace server {
class SectorHeartbeatMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 21902;

    void encode() override;
    int getMessageType() const override { return kMessageType; }

    void setTurn(int turn) { turn_ = turn; }
    void setChecksum(int checksum) { checksum_ = checksum; }
    void setCommands(const std::vector<std::vector<std::uint8_t>> &encodedCommands) { commands_ = encodedCommands; }

private:
    int turn_ = 0;
    int checksum_ = 0;
    std::vector<std::vector<std::uint8_t>> commands_; // LogicCommandManager::encodeCommand
};
} // namespace server
