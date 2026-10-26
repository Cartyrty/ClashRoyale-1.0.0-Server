#pragma once
// The requester's side of a donation: the card arrives on their home.
#include "logic/command/server/LogicServerCommand.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

#include <string>

namespace server {
class LogicAllianceUnitReceivedCommand : public LogicServerCommand {
public:
    static constexpr int kCommandType = 211;

    LogicAllianceUnitReceivedCommand(std::string donorName, int spellGlobalId)
        : donorName_(std::move(donorName)), spell_(spellGlobalId) {}
    int getCommandType() const override { return kCommandType; }

protected:
    void encodeFields(ByteStream &stream) const override {
        stream.writeString(donorName_);
        ByteStreamHelper::writeDataReference(stream, spell_);
    }

private:
    std::string donorName_;
    int spell_;
};
} // namespace server
