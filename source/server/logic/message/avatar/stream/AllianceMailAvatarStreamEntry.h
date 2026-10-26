#pragma once
// Clan mail from a leader or co-leader.
#include "logic/message/avatar/stream/AvatarStreamEntry.h"

namespace server {
class AllianceMailAvatarStreamEntry : public AvatarStreamEntry {
public:
    int getAvatarStreamEntryType() const override { return kAllianceMail; }

    void setAlliance(std::int64_t id, const std::string &name, int badge) { alliance_ = {id, name, badge}; }
    void setMail(const std::string &title, const std::string &message) {
        title_ = title;
        message_ = message;
    }

protected:
    void encodeFields(ByteStream &stream) const override {
        stream.writeString(message_);
        stream.writeBoolean(false);
        stream.writeString(title_);
        alliance_.encode(stream);
    }
    void saveFields(json::Value &saved) const override {
        saveOptional(saved, "message", message_);
        saveOptional(saved, "title", title_);
        alliance_.save(saved);
    }
    void loadFields(const json::Value &saved) override {
        message_ = loadOptional(saved, "message");
        title_ = loadOptional(saved, "title");
        alliance_.load(saved);
    }

private:
    std::optional<std::string> message_;
    std::optional<std::string> title_;
    AllianceReference alliance_;
};
} // namespace server
