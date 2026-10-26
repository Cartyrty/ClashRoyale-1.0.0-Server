#pragma once
// The battle log: the account, then its battle reports (newest first).
#include "logic/message/avatar/stream/BattleReportStreamEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class BattleReportStreamMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24413;

    void encode() override {
        stream_.writeLong(accountId_);
        stream_.writeVInt(int(entries_.size()));
        for (const auto &entry : entries_) entry.encode(stream_);
    }
    int getMessageType() const override { return kMessageType; }

    void setAccountId(std::int64_t accountId) { accountId_ = accountId; }
    void addBattleReport(BattleReportStreamEntry entry) { entries_.push_back(std::move(entry)); }

private:
    std::int64_t accountId_ = 0;
    std::vector<BattleReportStreamEntry> entries_;
};
} // namespace server
