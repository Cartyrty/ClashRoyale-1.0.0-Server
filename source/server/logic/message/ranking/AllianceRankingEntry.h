#pragma once
// A clan leaderboard row, built as the library's own AllianceRankingEntry (operator new(56), its constructor) with
// the server's values put in its fields; its encode writes it.
#include "library/Library.h"
#include "logic/data/LogicDataTables.h"
#include "logic/message/ranking/RankingEntry.h"

namespace server {
class AllianceRankingEntry : public RankingEntry {
public:
    void setBadge(int badge) { badge_ = badge; }
    void setRegion(int region) { region_ = region; }
    void setMemberCount(int count) { memberCount_ = count; }

    library::Ptr createLibraryEntry() const override {
        // AllianceRankingEntry (0x38 bytes): RankingEntry, member count +0x24, badge LogicAllianceBadgeData* +0x28,
        // region LogicRegionData* +0x30.
        constexpr std::size_t kSize = 0x38;
        constexpr int kMemberCountOffset = 0x24;
        constexpr int kBadgeOffset = 0x28;
        constexpr int kRegionOffset = 0x30;

        const auto entry = library::allocate(kSize);
        library::call("__ZN20AllianceRankingEntryC1Ev", {entry});
        fillRankingEntry(entry);
        library::writeInt(entry + kMemberCountOffset, memberCount_);
        library::writePointer(entry + kBadgeOffset, LogicDataTables::getDataById(badge_));
        library::writePointer(entry + kRegionOffset, LogicDataTables::getDataById(region_));
        return entry;
    }

private:
    int badge_ = 0;
    int region_ = 0;
    int memberCount_ = 0;
};
} // namespace server
