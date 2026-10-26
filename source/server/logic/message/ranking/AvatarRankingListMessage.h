#pragma once
// The global player leaderboard.
#include "logic/message/ranking/AvatarRankingEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class AvatarRankingListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24403;

    void encode() override {
        stream_.writeVInt(int(entries_.size()));
        for (const auto &entry : entries_) entry.encode(stream_);
    }
    int getMessageType() const override { return kMessageType; }

    void addRankingEntry(AvatarRankingEntry entry) { entries_.push_back(std::move(entry)); }

private:
    std::vector<AvatarRankingEntry> entries_;
};
} // namespace server
