#pragma once
// The player was kicked out of their clan, with the kicker's optional message.
#include "logic/message/avatar/stream/AvatarStreamEntry.h"

namespace server {
class AllianceKickOutStreamEntry : public AvatarStreamEntry {
public:
    int getAvatarStreamEntryType() const override { return kAllianceKickOut; }

    void setAlliance(std::int64_t id, const std::string &name, int badge) { alliance_ = {id, name, badge}; }
    void setMessage(const std::optional<std::string> &message) { message_ = message; }

protected:
    void encodeFields(ByteStream &stream) const override {
        stream.writeString(message_);
        alliance_.encode(stream);
        stream.writeBoolean(false);
    }
    void saveFields(json::Value &saved) const override {
        saveOptional(saved, "message", message_);
        alliance_.save(saved);
    }
    void loadFields(const json::Value &saved) override {
        message_ = loadOptional(saved, "message");
        alliance_.load(saved);
    }

private:
    std::optional<std::string> message_;
    AllianceReference alliance_;
};
} // namespace server
