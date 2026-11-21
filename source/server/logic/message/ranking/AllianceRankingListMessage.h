#pragma once
// The global clan leaderboard. Encoded by the library's own AllianceRankingListMessage.
#include "library/LibraryMessage.h"
#include "logic/message/ranking/AllianceRankingEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class AllianceRankingListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24401;

    void encode() override {
        // AllianceRankingListMessage: LogicArrayList<AllianceRankingEntry*>* +0x48.
        constexpr int kEntriesOffset = 0x48;
        auto message = library::LibraryMessage::create(kMessageType);
        const auto list = library::newArrayList();
        for (const auto &entry : entries_) library::addArrayItem("__ZN14LogicArrayListIP20AllianceRankingEntryE3addERKS1_", list, entry.createLibraryEntry());
        message.writePointer(kEntriesOffset, list);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void addRankingEntry(AllianceRankingEntry entry) { entries_.push_back(std::move(entry)); }

private:
    std::vector<AllianceRankingEntry> entries_;
};
} // namespace server
