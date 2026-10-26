#pragma once
// A player leaderboard row: RankingEntry, then VInt exp level, four VInts, String, Long home id, Boolean in a clan
// (+ Long clan id, String clan name, clan badge).
#include "logic/message/ranking/RankingEntry.h"

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

protected:
    void encodeFields(ByteStream &stream) const override {
        stream.writeVInt(expLevel_);
        for (int i = 0; i < 4; ++i) stream.writeVInt(0);
        stream.writeString(std::nullopt);
        stream.writeLong(homeId_);
        stream.writeBoolean(hasAlliance_);
        if (!hasAlliance_) return;
        stream.writeLong(allianceId_);
        stream.writeString(allianceName_);
        ByteStreamHelper::writeDataReference(stream, allianceBadge_);
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
