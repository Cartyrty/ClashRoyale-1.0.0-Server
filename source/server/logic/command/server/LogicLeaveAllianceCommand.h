#pragma once
// The avatar leaves its alliance (or was kicked out).
#include "logic/command/server/LogicServerCommand.h"

#include <cstdint>

namespace server {
class LogicLeaveAllianceCommand : public LogicServerCommand {
public:
    static constexpr int kCommandType = 208;

    LogicLeaveAllianceCommand(std::int64_t allianceId, bool kicked) : allianceId_(allianceId), kicked_(kicked) {}
    int getCommandType() const override { return kCommandType; }

protected:
    void encodeFields(ByteStream &stream) const override {
        stream.writeLong(allianceId_);
        stream.writeBoolean(kicked_);
    }

private:
    std::int64_t allianceId_;
    bool kicked_;
};
} // namespace server
