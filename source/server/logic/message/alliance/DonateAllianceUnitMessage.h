#pragma once
// A card donated to a request. Decoded by the library's own DonateAllianceUnitMessage.
#include "library/LibraryMessage.h"
#include "logic/data/LogicData.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class DonateAllianceUnitMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14310;

    void decode() override {
        // DonateAllianceUnitMessage: spell LogicSpellData* +0x48, request stream entry id LogicLong* +0x50.
        constexpr int kSpellOffset = 0x48;
        constexpr int kStreamEntryIdOffset = 0x50;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        spell_ = LogicData::getGlobalID(message.readPointer(kSpellOffset));
        streamEntryId_ = message.readLong(kStreamEntryIdOffset);
    }
    int getMessageType() const override { return kMessageType; }

    int getSpell() const { return spell_; }
    std::int64_t getStreamEntryId() const { return streamEntryId_; } // the request

private:
    int spell_ = 0;
    std::int64_t streamEntryId_ = 0;
};
} // namespace server
