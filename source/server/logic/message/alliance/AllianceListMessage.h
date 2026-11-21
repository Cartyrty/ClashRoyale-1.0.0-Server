#pragma once
// Clan search results: the search string, then the clans. Encoded by the library's own AllianceListMessage.
#include "library/LibraryMessage.h"
#include "logic/message/alliance/AllianceHeaderEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <string>
#include <vector>

namespace server {
class AllianceListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24310;

    void encode() override {
        // AllianceListMessage: search string String* +0x48, LogicArrayList<AllianceHeaderEntry*>* +0x50.
        constexpr int kSearchStringOffset = 0x48;
        constexpr int kAlliancesOffset = 0x50;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeString(kSearchStringOffset, searchString_);
        const auto list = library::newArrayList();
        for (const auto &entry : alliances_)
            library::addArrayItem("__ZN14LogicArrayListIP19AllianceHeaderEntryE3addERKS1_", list,
                                  entry.createLibraryEntry());
        message.writePointer(kAlliancesOffset, list);
        stream_.writeBytesWithoutLength(message.encode());
    }
    int getMessageType() const override { return kMessageType; }

    void setSearchString(const std::string &searchString) { searchString_ = searchString; }
    void addAlliance(AllianceHeaderEntry entry) { alliances_.push_back(std::move(entry)); }

private:
    std::string searchString_;
    std::vector<AllianceHeaderEntry> alliances_;
};
} // namespace server
