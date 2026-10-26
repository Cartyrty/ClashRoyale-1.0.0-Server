#pragma once
// Clan search results: the search string, then the clans.
#include "logic/message/alliance/AllianceHeaderEntry.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <vector>

namespace server {
class AllianceListMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24310;

    void encode() override {
        stream_.writeStringReference(searchString_);
        stream_.writeVInt(int(alliances_.size()));
        for (const auto &entry : alliances_) entry.encode(stream_);
    }
    int getMessageType() const override { return kMessageType; }

    void setSearchString(const std::string &searchString) { searchString_ = searchString; }
    void addAlliance(AllianceHeaderEntry entry) { alliances_.push_back(std::move(entry)); }

private:
    std::string searchString_;
    std::vector<AllianceHeaderEntry> alliances_;
};
} // namespace server
