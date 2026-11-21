#pragma once
// A request to join an invite-only clan, and how it was answered.
#include "logic/message/alliance/stream/StreamEntry.h"

#include <string>

namespace server {
class JoinRequestAllianceStreamEntry : public StreamEntry {
public:
    static constexpr int kStatePending = 1;
    static constexpr int kStateAccepted = 2;
    static constexpr int kStateRefused = 3;

    int getStreamEntryType() const override { return kJoinRequest; }

    void setMessage(const std::string &message) { message_ = message; }
    int getState() const { return state_; }
    void setState(int state) { state_ = state; }
    void setResponderName(const std::string &name) { responderName_ = name; }

protected:
    void fillLibraryEntry(library::Ptr entry) const override {
        // JoinRequestAllianceStreamEntry (80 bytes): message String* +0x38, state +0x40 (1 by default), responder
        // name String* +0x48.
        constexpr int kMessageOffset = 0x38;
        constexpr int kStateOffset = 0x40;
        constexpr int kResponderNameOffset = 0x48;
        library::writePointer(entry + kMessageOffset, library::newString(message_));
        library::writePointer(entry + kResponderNameOffset, library::newString(responderName_));
        library::writeInt(entry + kStateOffset, state_);
    }
    void saveFields(json::Value &saved) const override {
        saved["text"] = message_;
        saved["responderName"] = responderName_;
        saved["state"] = state_;
    }
    void loadFields(const json::Value &saved) override {
        message_ = saved["text"].str("");
        responderName_ = saved["responderName"].str("");
        state_ = int(saved["state"].integer(kStatePending));
    }

private:
    std::string message_;
    std::string responderName_;
    int state_ = kStatePending;
};
} // namespace server
