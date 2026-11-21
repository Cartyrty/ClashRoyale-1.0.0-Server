#pragma once
// A clan's page. Encoded by the library's own AllianceDataMessage.
#include "library/LibraryMessage.h"
#include "logic/message/alliance/AllianceFullEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class AllianceDataMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24301;

    void encode() override {
        // AllianceDataMessage: AllianceFullEntry* +0x48 (setAllianceFullEntry; the message frees it).
        constexpr int kAllianceFullEntryOffset = 0x48;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writePointer(kAllianceFullEntryOffset, alliance_.createLibraryEntry());
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void setAllianceFullEntry(AllianceFullEntry alliance) { alliance_ = std::move(alliance); }

private:
    AllianceFullEntry alliance_;
};
} // namespace server
