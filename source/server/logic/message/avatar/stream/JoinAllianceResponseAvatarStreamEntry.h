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
    void fillLibraryEntry(library::Ptr entry) const override {
        // JoinAllianceResponseAvatarStreamEntry (96 bytes): alliance id LogicLong* +0x30, alliance name String* +0x38,
        // badge +0x40, accepted bool +0x48, LogicLong* +0x50 (null: none), message String* +0x58.
        constexpr int kAcceptedOffset = 0x48;
        constexpr int kMessageOffset = 0x58;
        alliance_.fill(entry, 0x30, 0x38, 0x40);
        if (message_) library::writePointer(entry + kMessageOffset, library::newString(*message_));
        library::writeByte(entry + kAcceptedOffset, accepted_ ? 1 : 0);
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
