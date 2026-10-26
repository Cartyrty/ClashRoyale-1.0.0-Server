#pragma once
// A new inbox entry.
#include "logic/message/avatar/stream/AvatarStreamEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AvatarStreamEntryMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24412;

    void encode() override { entry_->encode(stream_); }
    int getMessageType() const override { return kMessageType; }

    // Not copied: the entry outlives the message.
    void setStreamEntry(const AvatarStreamEntry &entry) { entry_ = &entry; }

private:
    const AvatarStreamEntry *entry_ = nullptr;
};
} // namespace server
