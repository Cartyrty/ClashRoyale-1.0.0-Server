#pragma once
// The avatar's alliance changed its settings (the badge the avatar shows).
#include "logic/command/server/LogicServerCommand.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

#include <cstdint>

namespace server {
class LogicAllianceSettingsChangedCommand : public LogicServerCommand {
public:
    static constexpr int kCommandType = 212;

    LogicAllianceSettingsChangedCommand(std::int64_t allianceId, int badgeGlobalId)
        : allianceId_(allianceId), badge_(badgeGlobalId) {}
    int getCommandType() const override { return kCommandType; }

protected:
    void encodeFields(ByteStream &stream) const override {
        stream.writeLong(allianceId_);
        ByteStreamHelper::writeDataReference(stream, badge_);
    }

private:
    std::int64_t allianceId_;
    int badge_;
};
} // namespace server
