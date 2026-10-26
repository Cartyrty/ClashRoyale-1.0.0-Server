#pragma once
// The avatar's role in its alliance changed.
#include "logic/command/server/LogicServerCommand.h"

#include <cstdint>

namespace server {
class LogicChangeAllianceRoleCommand : public LogicServerCommand {
public:
    static constexpr int kCommandType = 210;

    LogicChangeAllianceRoleCommand(std::int64_t allianceId, int role) : allianceId_(allianceId), role_(role) {}
    int getCommandType() const override { return kCommandType; }

protected:
    void encodeFields(ByteStream &stream) const override {
        stream.writeLong(allianceId_);
        stream.writeVInt(role_);
    }

private:
    std::int64_t allianceId_;
    int role_;
};
} // namespace server
