#pragma once
// A player leaderboard row, built as the library's own AvatarRankingEntry (operator new(96), its constructor) with
// the server's values put in its fields; its encode writes it.
#include "library/Library.h"
#include "logic/data/LogicDataTables.h"
#include "logic/message/ranking/RankingEntry.h"

#include <cstdint>
#include <string>

namespace server {
class AvatarRankingEntry : public RankingEntry {
public:
    void setExpLevel(int level) { expLevel_ = level; }
    void setHomeId(std::int64_t id) { homeId_ = id; }
    void setAlliance(std::int64_t id, const std::string &name, int badge) {
        hasAlliance_ = true;
        allianceId_ = id;
        allianceName_ = name;
        allianceBadge_ = badge;
    }

    library::Ptr createLibraryEntry() const override {
        // AvatarRankingEntry (0x60 bytes): RankingEntry, exp level +0x24, four VInts +0x28..+0x34, String* +0x38,
        // home id LogicLong* +0x40, alliance id LogicLong* +0x48 (null: not in a clan), alliance name String* +0x50,
        // alliance badge LogicAllianceBadgeData* +0x58.
        constexpr std::size_t kSize = 0x60;
        constexpr int kExpLevelOffset = 0x24;
        constexpr int kHomeIdOffset = 0x40;
        constexpr int kAllianceIdOffset = 0x48;
        constexpr int kAllianceNameOffset = 0x50;
        constexpr int kAllianceBadgeOffset = 0x58;

        const auto entry = library::allocate(kSize);
        library::call("__ZN18AvatarRankingEntryC1Ev", {entry});
        fillRankingEntry(entry);
        library::writeInt(entry + kExpLevelOffset, expLevel_);
        library::writePointer(entry + kHomeIdOffset, library::newLogicLong(homeId_));
        if (hasAlliance_) {
            library::writePointer(entry + kAllianceIdOffset, library::newLogicLong(allianceId_));
            library::writePointer(entry + kAllianceNameOffset, library::newString(allianceName_));
            library::writePointer(entry + kAllianceBadgeOffset, LogicDataTables::getDataById(allianceBadge_));
        }
        return entry;
    }

private:
    int expLevel_ = 1;
    std::int64_t homeId_ = 0;
    bool hasAlliance_ = false;
    std::int64_t allianceId_ = 0;
    std::string allianceName_;
    int allianceBadge_ = 0;
};
} // namespace server
