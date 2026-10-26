#pragma once
// New clan settings. Payload: String description, data reference badge, VInt type, VInt requiredScore, data reference
// region.
#include "titan/logic/datastream/ByteStreamHelper.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class ChangeAllianceSettingsMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14316;

    void decode() override {
        description_ = stream_.readString();
        badge_ = ByteStreamHelper::readDataReference(stream_);
        type_ = stream_.readVInt();
        requiredScore_ = stream_.readVInt();
        region_ = ByteStreamHelper::readDataReference(stream_);
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
