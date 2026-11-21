#pragma once
// The battle log: the account, then its battle reports (newest first). Encoded by the library's own
// BattleReportStreamMessage.
#include "library/LibraryMessage.h"
#include "logic/message/avatar/stream/BattleReportStreamEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <vector>

namespace server {
class BattleReportStreamMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24413;

    void encode() override {
        // BattleReportStreamMessage: account id LogicLong* +0x48, LogicArrayList<BattleReportStreamEntry*>* +0x50.
        constexpr int kAccountIdOffset = 0x48;
        constexpr int kEntriesOffset = 0x50;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeLong(kAccountIdOffset, accountId_);
        const auto list = library::newArrayList();
        for (const auto &entry : entries_)
            library::addArrayItem("__ZN14LogicArrayListIP23BattleReportStreamEntryE3addERKS1_", list,
                                  entry.createLibraryEntry());
        message.writePointer(kEntriesOffset, list);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void setAccountId(std::int64_t accountId) { accountId_ = accountId; }
    void addBattleReport(BattleReportStreamEntry entry) { entries_.push_back(std::move(entry)); }

private:
    std::int64_t accountId_ = 0;
    std::vector<BattleReportStreamEntry> entries_;
};
} // namespace server
