#pragma once
// A clan leaderboard row: RankingEntry, then the badge, the region and VInt member count.
#include "logic/message/ranking/RankingEntry.h"

namespace server {
class AllianceRankingEntry : public RankingEntry {
public:
    void setBadge(int badge) { badge_ = badge; }
    void setRegion(int region) { region_ = region; }
    void setMemberCount(int count) { memberCount_ = count; }

protected:
    void encodeFields(ByteStream &stream) const override {
        ByteStreamHelper::writeDataReference(stream, badge_);
        ByteStreamHelper::writeDataReference(stream, region_);
        stream.writeVInt(memberCount_);
    }

private:
    int badge_ = 0;
    int region_ = 0;
    int memberCount_ = 0;
};
} // namespace server
