#pragma once
// The donor's side of a donation: the card leaves their home and the donation reward is paid.
#include "logic/command/server/LogicServerCommand.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

#include <cstdint>

namespace server {
class LogicDonateAllianceUnitCommand : public LogicServerCommand {
public:
    static constexpr int kCommandType = 203;

    LogicDonateAllianceUnitCommand(int spellGlobalId, std::int64_t streamEntryId)
        : spell_(spellGlobalId), streamEntryId_(streamEntryId) {}
    int getCommandType() const override { return kCommandType; }

protected:
    void encodeFields(ByteStream &stream) const override {
        ByteStreamHelper::writeDataReference(stream, spell_);
        stream.writeLong(streamEntryId_);
    }

private:
    int spell_;
    std::int64_t streamEntryId_;
};
} // namespace server
