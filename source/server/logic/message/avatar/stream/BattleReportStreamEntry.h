#pragma once
// A battle log entry (EventScreen::refreshBattleLogItems lists them): the library's LogicBattleLog JSON, the client
// version it was played with, its replay and arena. Battle reports live in the battle_reports table, not in the avatar
// stream, and are sent as the library's own BattleReportStreamEntry (AvatarStreamEntryFactory type 2, the server's
// values put in its fields), which BattleReportStreamMessage encodes.
#include "game/utils/TimeUtil.h"
#include "library/Library.h"
#include "logic/data/LogicDataTables.h"
#include "logic/message/account/LoginOkMessage.h"
#include "logic/message/avatar/stream/AvatarStreamEntry.h"

#include <cstdint>
#include <string>

namespace server {
struct BattleReportStreamEntry {
    std::int64_t id = 0;
    std::int64_t opponentAvatarId = 0;
    std::string opponentName;
    int opponentExpLevel = 1;
    int ageSeconds = 0;
    bool isNew = true;
    std::string battleLogJSON;
    std::int64_t replayId = 0;
    int arena = 0;

    // A new library BattleReportStreamEntry; its owner (BattleReportStreamMessage's list) frees it.
    library::Ptr createLibraryEntry() const {
        // BattleReportStreamEntry (96 bytes): AvatarStreamEntry (id LogicLong* +0x08, sender LogicLong* +0x10, sender
        // name String* +0x18, sender level +0x20, age +0x24, removed +0x28, new +0x29), type +0x2c, battle log
        // String* +0x30, bool +0x38, replay shard +0x3c, replay id LogicLong* +0x40 (null: no replay), major / build /
        // content version +0x48 / +0x4c / +0x50, arena LogicArenaData* +0x58.
        constexpr int kIdOffset = 0x08;
        constexpr int kSenderAvatarIdOffset = 0x10;
        constexpr int kSenderNameOffset = 0x18;
        constexpr int kSenderExpLevelOffset = 0x20;
        constexpr int kAgeSecondsOffset = 0x24;
        constexpr int kNewOffset = 0x29;
        constexpr int kBattleLogOffset = 0x30;
        constexpr int kReplayShardOffset = 0x3c;
        constexpr int kReplayIdOffset = 0x40;
        constexpr int kMajorVersionOffset = 0x48;
        constexpr int kBuildOffset = 0x4c;
        constexpr int kContentVersionOffset = 0x50;
        constexpr int kArenaOffset = 0x58;
        constexpr int kReplayShard = 0;

        const auto entry = library::call("__ZN24AvatarStreamEntryFactory23createStreamEntryByTypeEi",
                                         {std::uint64_t(AvatarStreamEntry::kBattleReport)});
        library::writePointer(entry + kIdOffset, library::newLogicLong(id));
        library::writePointer(entry + kSenderAvatarIdOffset, library::newLogicLong(opponentAvatarId));
        library::writePointer(entry + kSenderNameOffset, library::newString(opponentName));
        library::writeInt(entry + kSenderExpLevelOffset, opponentExpLevel);
        library::writeInt(entry + kAgeSecondsOffset, ageSeconds);
        library::writeByte(entry + kNewOffset, isNew ? 1 : 0);
        library::writePointer(entry + kBattleLogOffset, library::newString(battleLogJSON));
        library::writeInt(entry + kMajorVersionOffset, LoginOkMessage::kServerMajorVersion);
        library::writeInt(entry + kBuildOffset, LoginOkMessage::kServerBuild);
        library::writeInt(entry + kContentVersionOffset, LoginOkMessage::kContentVersion);
        library::writeInt(entry + kReplayShardOffset, kReplayShard);
        library::writePointer(entry + kReplayIdOffset, library::newLogicLong(replayId));
        library::writePointer(entry + kArenaOffset, LogicDataTables::getDataById(arena));
        return entry;
    }
};
} // namespace server
