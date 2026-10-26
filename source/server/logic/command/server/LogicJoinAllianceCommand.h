#pragma once
// The avatar joins an alliance (or created it: the creation cost is paid on the home).
#include "logic/command/server/LogicServerCommand.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

#include <cstdint>
#include <string>

namespace server {
class LogicJoinAllianceCommand : public LogicServerCommand {
public:
    static constexpr int kCommandType = 209;

    LogicJoinAllianceCommand(std::int64_t allianceId, std::string allianceName, int badgeGlobalId, bool created)
        : allianceId_(allianceId), allianceName_(std::move(allianceName)), badge_(badgeGlobalId), created_(created) {}
    int getCommandType() const override { return kCommandType; }

protected:
    void encodeFields(ByteStream &stream) const override {
        stream.writeLong(allianceId_);
        stream.writeString(allianceName_);
        ByteStreamHelper::writeDataReference(stream, badge_);
        stream.writeBoolean(created_);
    }

private:
    std::int64_t allianceId_;
    std::string allianceName_;
    int badge_;
    bool created_;
};
} // namespace server
