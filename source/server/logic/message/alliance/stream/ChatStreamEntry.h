#pragma once
// A clan chat message.
#include "logic/message/alliance/stream/StreamEntry.h"

#include <string>

namespace server {
class ChatStreamEntry : public StreamEntry {
public:
    int getStreamEntryType() const override { return kChat; }

    void setMessage(const std::string &message) { message_ = message; }

protected:
    void fillLibraryEntry(library::Ptr entry) const override {
        // ChatStreamEntry (64 bytes): message String* +0x38.
        constexpr int kMessageOffset = 0x38;
        library::writePointer(entry + kMessageOffset, library::newString(message_));
    }
    void saveFields(json::Value &saved) const override { saved["text"] = message_; }
    void loadFields(const json::Value &saved) override { message_ = saved["text"].str(""); }

private:
    std::string message_;
};
} // namespace server
