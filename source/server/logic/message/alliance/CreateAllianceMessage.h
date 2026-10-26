#pragma once
// A new clan. Payload: String(60) name, String(560) description, data reference badge, VInt type, VInt requiredScore,
// data reference region.
#include "titan/logic/datastream/ByteStreamHelper.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class CreateAllianceMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14301;

    void decode() override {
        name_ = stream_.readString(60);
        description_ = stream_.readString(560);
        badge_ = ByteStreamHelper::readDataReference(stream_);
        type_ = stream_.readVInt();
        requiredScore_ = stream_.readVInt();
        region_ = ByteStreamHelper::readDataReference(stream_);
    }
    int getMessageType() const override { return kMessageType; }

    const std::optional<std::string> &getName() const { return name_; }
    const std::optional<std::string> &getDescription() const { return description_; }
    int getBadge() const { return badge_; } // alliance_badges.csv
    int getType() const { return type_; }   // 1 open, 2 invite only, 3 closed
    int getRequiredScore() const { return requiredScore_; }
    int getRegion() const { return region_; } // regions.csv

private:
    std::optional<std::string> name_;
    std::optional<std::string> description_;
    int badge_ = 0;
    int type_ = 0;
    int requiredScore_ = 0;
    int region_ = 0;
};
} // namespace server
