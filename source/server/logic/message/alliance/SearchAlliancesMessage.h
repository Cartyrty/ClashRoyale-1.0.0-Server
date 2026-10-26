#pragma once
// A clan search. Payload: String(60) searchString, data reference region, Int minMembers, Int maxMembers, Int minScore,
// Boolean onlyJoinable.
#include "titan/logic/datastream/ByteStreamHelper.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class SearchAlliancesMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14324;

    void decode() override {
        searchString_ = stream_.readString(60);
        region_ = ByteStreamHelper::readDataReference(stream_);
        minMembers_ = stream_.readInt();
        maxMembers_ = stream_.readInt();
        minScore_ = stream_.readInt();
        onlyJoinable_ = stream_.readBoolean();
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
