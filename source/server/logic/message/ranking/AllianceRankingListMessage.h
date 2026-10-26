#pragma once
// The global clan leaderboard.
#include "logic/message/ranking/AllianceRankingEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class AllianceRankingListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24401;

    void encode() override {
        stream_.writeVInt(int(entries_.size()));
        for (const auto &entry : entries_) entry.encode(stream_);
    }
    int getMessageType() const override { return kMessageType; }

    void addRankingEntry(AllianceRankingEntry entry) { entries_.push_back(std::move(entry)); }

private:
    std::vector<AllianceRankingEntry> entries_;
};
} // namespace server
