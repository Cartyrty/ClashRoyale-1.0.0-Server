#pragma once
// A new clan. Decoded by the library's own CreateAllianceMessage.
#include "library/LibraryMessage.h"
#include "logic/data/LogicData.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class CreateAllianceMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14301;

    void decode() override {
        // CreateAllianceMessage: name String* +0x48 (at most 60), description String* +0x50 (at most 560), badge
        // LogicAllianceBadgeData* +0x58, region LogicRegionData* +0x60, type +0x68, required score +0x6c.
        constexpr int kNameOffset = 0x48;
        constexpr int kDescriptionOffset = 0x50;
        constexpr int kBadgeOffset = 0x58;
        constexpr int kRegionOffset = 0x60;
        constexpr int kTypeOffset = 0x68;
        constexpr int kRequiredScoreOffset = 0x6c;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        name_ = message.readString(kNameOffset);
        description_ = message.readString(kDescriptionOffset);
        badge_ = LogicData::getGlobalID(message.readPointer(kBadgeOffset));
        type_ = message.readInt(kTypeOffset);
        requiredScore_ = message.readInt(kRequiredScoreOffset);
        region_ = LogicData::getGlobalID(message.readPointer(kRegionOffset));
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
