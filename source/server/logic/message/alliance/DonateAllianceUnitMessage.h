#pragma once
// A card donated to a request. Payload: data reference spell, Long streamEntryId.
#include "titan/logic/datastream/ByteStreamHelper.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class DonateAllianceUnitMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14310;

    void decode() override {
        spell_ = ByteStreamHelper::readDataReference(stream_);
        streamEntryId_ = stream_.readLong();
    }
    int getMessageType() const override { return kMessageType; }

    int getSpell() const { return spell_; }
    std::int64_t getStreamEntryId() const { return streamEntryId_; } // the request

private:
    int spell_ = 0;
    std::int64_t streamEntryId_ = 0;
};
} // namespace server
