#pragma once
// The player's whole inbox (avatar stream).
#include "logic/message/avatar/stream/AvatarStreamEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class AvatarStreamMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24411;

    void encode() override {
        stream_.writeVInt(int(entries_.size()));
        for (const auto &entry : entries_) entry->encode(stream_);
    }
    int getMessageType() const override { return kMessageType; }

    // Not copied: the entry outlives the message.
    void addStreamEntry(const AvatarStreamEntry &entry) { entries_.push_back(&entry); }

private:
    std::vector<const AvatarStreamEntry *> entries_;
};
} // namespace server
