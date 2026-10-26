#pragma once
// The whole clan stream (chat, requests, events).
#include "logic/message/alliance/stream/StreamEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class AllianceStreamMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24311;

    void encode() override {
        stream_.writeVInt(int(entries_.size()));
        for (const auto &entry : entries_) entry->encode(stream_);
    }
    int getMessageType() const override { return kMessageType; }

    // Not copied: the entry outlives the message.
    void addStreamEntry(const StreamEntry &entry) { entries_.push_back(&entry); }

private:
    std::vector<const StreamEntry *> entries_;
};
} // namespace server
