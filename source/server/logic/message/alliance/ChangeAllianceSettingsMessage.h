#pragma once
// New clan settings. Decoded by the library's own ChangeAllianceSettingsMessage.
#include "library/LibraryMessage.h"
#include "logic/data/LogicData.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class ChangeAllianceSettingsMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14316;

    void decode() override {
        // ChangeAllianceSettingsMessage: description String* +0x48, badge LogicAllianceBadgeData* +0x50, region
        // LogicRegionData* +0x58, type +0x60, required score +0x64.
        constexpr int kDescriptionOffset = 0x48;
        constexpr int kBadgeOffset = 0x50;
        constexpr int kRegionOffset = 0x58;
        constexpr int kTypeOffset = 0x60;
        constexpr int kRequiredScoreOffset = 0x64;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        description_ = message.readString(kDescriptionOffset);
        badge_ = LogicData::getGlobalID(message.readPointer(kBadgeOffset));
        type_ = message.readInt(kTypeOffset);
        requiredScore_ = message.readInt(kRequiredScoreOffset);
        region_ = LogicData::getGlobalID(message.readPointer(kRegionOffset));
    }
    int getMessageType() const override { return kMessageType; }

    const std::optional<std::string> &getDescription() const { return description_; }
    int getBadge() const { return badge_; }
    int getType() const { return type_; }
    int getRequiredScore() const { return requiredScore_; }
    int getRegion() const { return region_; }

private:
    std::optional<std::string> description_;
    int badge_ = 0;
    int type_ = 0;
    int requiredScore_ = 0;
    int region_ = 0;
};
} // namespace server
