#pragma once
// A new or changed clan stream entry.
#include "logic/message/alliance/stream/StreamEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceStreamEntryMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24312;

    void encode() override { entry_->encode(stream_); }
    int getMessageType() const override { return kMessageType; }

    // Not copied: the entry outlives the message.
    void setStreamEntry(const StreamEntry &entry) { entry_ = &entry; }

private:
    const StreamEntry *entry_ = nullptr;
};
} // namespace server
