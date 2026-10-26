#pragma once
// A battle log entry (EventScreen::refreshBattleLogItems lists them): the library's LogicBattleLog JSON, the client
// version it was played with, its replay and arena. Battle reports live in the battle_reports table, not in the avatar
// stream, and are encoded as BattleReportStreamEntry::decode reads them: VInt type, Long id, Long sender, String sender
// name, VInt sender level, VInt age, Boolean removed, Boolean new, String battle log, Boolean, VInt major / build /
// content version, Boolean replay present (+ VInt shard, Long replay id), arena.
#include "logic/message/account/LoginOkMessage.h"
#include "logic/message/avatar/stream/AvatarStreamEntry.h"
#include "titan/logic/datastream/ByteStream.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

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

    void encode(ByteStream &stream) const {
        constexpr int kReplayShard = 0;
        stream.writeVInt(AvatarStreamEntry::kBattleReport);
        stream.writeLong(id);
        stream.writeLong(opponentAvatarId);
        stream.writeString(opponentName);
        stream.writeVInt(opponentExpLevel);
        stream.writeVInt(ageSeconds);
        stream.writeBoolean(false);
        stream.writeBoolean(isNew);
        stream.writeString(battleLogJSON);
        stream.writeBoolean(false);
        stream.writeVInt(LoginOkMessage::kServerMajorVersion);
        stream.writeVInt(LoginOkMessage::kServerBuild);
        stream.writeVInt(LoginOkMessage::kContentVersion);
        stream.writeBoolean(true);
        stream.writeVInt(kReplayShard);
        stream.writeLong(replayId);
        ByteStreamHelper::writeDataReference(stream, arena);
    }
};
} // namespace server
