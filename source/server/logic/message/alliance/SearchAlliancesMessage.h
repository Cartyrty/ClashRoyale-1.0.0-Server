#pragma once
// A clan search. Decoded by the library's own SearchAlliancesMessage.
#include "library/LibraryMessage.h"
#include "logic/data/LogicData.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class SearchAlliancesMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14324;

    void decode() override {
        // SearchAlliancesMessage: search string String* +0x48, region LogicRegionData* +0x50, min members +0x58, max
        // members +0x5c (50 by default), min score +0x60, an Int +0x64, only joinable bool +0x68, an Int +0x6c.
        constexpr int kSearchStringOffset = 0x48;
        constexpr int kRegionOffset = 0x50;
        constexpr int kMinMembersOffset = 0x58;
        constexpr int kMaxMembersOffset = 0x5c;
        constexpr int kMinScoreOffset = 0x60;
        constexpr int kOnlyJoinableOffset = 0x68;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        searchString_ = message.readString(kSearchStringOffset);
        region_ = LogicData::getGlobalID(message.readPointer(kRegionOffset));
        minMembers_ = message.readInt(kMinMembersOffset);
        maxMembers_ = message.readInt(kMaxMembersOffset);
        minScore_ = message.readInt(kMinScoreOffset);
        onlyJoinable_ = message.readBoolean(kOnlyJoinableOffset);
    }
    int getMessageType() const override { return kMessageType; }

    const std::optional<std::string> &getSearchString() const { return searchString_; }
    int getRegion() const { return region_; } // 0: any
    int getMinMembers() const { return minMembers_; }
    int getMaxMembers() const { return maxMembers_; }
    int getMinScore() const { return minScore_; }
    bool getOnlyJoinable() const { return onlyJoinable_; }

private:
    std::optional<std::string> searchString_;
    int region_ = 0;
    int minMembers_ = 0;
    int maxMembers_ = 0;
    int minScore_ = 0;
    bool onlyJoinable_ = false;
};
} // namespace server
