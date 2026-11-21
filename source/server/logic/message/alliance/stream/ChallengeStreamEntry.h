#pragma once
// A friendly battle challenge; closed once someone accepted it.
#include "logic/message/alliance/stream/StreamEntry.h"

#include <string>

namespace server {
class ChallengeStreamEntry : public StreamEntry {
public:
    int getStreamEntryType() const override { return kChallenge; }

    void setMessage(const std::string &message) { message_ = message; }
    bool isClosed() const { return closed_; }
    void setClosed(bool closed) { closed_ = closed; }
    bool isOpen() const { return !isRemoved() && !closed_; }

protected:
    void fillLibraryEntry(library::Ptr entry) const override {
        // ChallengeStreamEntry (80 bytes): message String* +0x38, closed bool +0x40, String* +0x48 (null: none).
        constexpr int kMessageOffset = 0x38;
        constexpr int kClosedOffset = 0x40;
        library::writePointer(entry + kMessageOffset, library::newString(message_));
        library::writeByte(entry + kClosedOffset, closed_ ? 1 : 0);
    }
    void saveFields(json::Value &saved) const override {
        saved["text"] = message_;
        saved["closed"] = closed_;
    }
    void loadFields(const json::Value &saved) override {
        message_ = saved["text"].str("");
        closed_ = saved["closed"].boolean();
    }

private:
    std::string message_;
    bool closed_ = false;
};
} // namespace server
