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
    void fillLibraryEntry(library::Ptr entry) const override {
        // AllianceKickOutStreamEntry (88 bytes): message String* +0x30, alliance id LogicLong* +0x38, alliance name
        // String* +0x40, badge +0x48, LogicLong* +0x50 (null: none).
        constexpr int kMessageOffset = 0x30;
        if (message_) library::writePointer(entry + kMessageOffset, library::newString(*message_));
        alliance_.fill(entry, 0x38, 0x40, 0x48);
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
