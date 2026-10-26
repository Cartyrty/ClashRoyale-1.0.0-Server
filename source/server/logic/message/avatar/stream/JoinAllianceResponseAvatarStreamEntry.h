#pragma once
// The answer to the player's join request.
#include "logic/message/avatar/stream/AvatarStreamEntry.h"

namespace server {
class JoinAllianceResponseAvatarStreamEntry : public AvatarStreamEntry {
public:
    int getAvatarStreamEntryType() const override { return kJoinAllianceResponse; }

    void setAlliance(std::int64_t id, const std::string &name, int badge) { alliance_ = {id, name, badge}; }
    void setAccepted(bool accepted) { accepted_ = accepted; }

protected:
    void encodeFields(ByteStream &stream) const override {
        alliance_.encode(stream);
        stream.writeString(message_);
        stream.writeBoolean(accepted_);
        stream.writeBoolean(false);
    }
    void saveFields(json::Value &saved) const override {
        alliance_.save(saved);
        saveOptional(saved, "message", message_);
        saved["accepted"] = accepted_;
    }
    void loadFields(const json::Value &saved) override {
        alliance_.load(saved);
        message_ = loadOptional(saved, "message");
        accepted_ = saved["accepted"].boolean();
    }

private:
    AllianceReference alliance_;
    std::optional<std::string> message_;
    bool accepted_ = false;
};
} // namespace server
