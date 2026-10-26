#pragma once
// A clan's page: its header, its description and its members.
#include "logic/message/alliance/AllianceHeaderEntry.h"
#include "logic/message/alliance/AllianceMemberEntry.h"

#include <string>
#include <vector>

namespace server {
struct AllianceFullEntry {
    AllianceHeaderEntry header;
    std::string description;
    std::vector<AllianceMemberEntry> members;

    void encode(ByteStream &stream) const {
        header.encode(stream);
        stream.writeStringReference(description);
        stream.writeVInt(int(members.size()));
        for (const auto &member : members) member.encode(stream);
    }
};
} // namespace server
