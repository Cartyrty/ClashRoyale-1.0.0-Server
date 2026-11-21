#pragma once
// The local player leaderboard. Encoded by the library's own AvatarLocalRankingListMessage.
#include "library/LibraryMessage.h"
#include "logic/message/ranking/AvatarRankingEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class AvatarLocalRankingListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24404;

    void encode() override {
        // AvatarLocalRankingListMessage: LogicArrayList<AvatarRankingEntry*>* +0x48.
        constexpr int kEntriesOffset = 0x48;
        auto message = library::LibraryMessage::create(kMessageType);
        const auto list = library::newArrayList();
        for (const auto &entry : entries_) library::addArrayItem("__ZN14LogicArrayListIP18AvatarRankingEntryE3addERKS1_", list, entry.createLibraryEntry());
        message.writePointer(kEntriesOffset, list);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void addRankingEntry(AvatarRankingEntry entry) { entries_.push_back(std::move(entry)); }

private:
    std::vector<AvatarRankingEntry> entries_;
};
} // namespace server
