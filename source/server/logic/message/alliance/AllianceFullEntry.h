#pragma once
// A clan's page (its header, its description and its members), built as the library's own AllianceFullEntry
// (operator new(40), its constructor) with the server's values put in its fields; AllianceFullEntry::encode writes it.
#include "library/Library.h"
#include "logic/message/alliance/AllianceHeaderEntry.h"
#include "logic/message/alliance/AllianceMemberEntry.h"

#include <string>
#include <vector>

namespace server {
struct AllianceFullEntry {
    AllianceHeaderEntry header;
    std::string description;
    std::vector<AllianceMemberEntry> members;

    // A new library AllianceFullEntry; its owner (AllianceDataMessage) frees it, with its header and members.
    library::Ptr createLibraryEntry() const {
        // AllianceFullEntry (0x28 bytes): header AllianceHeaderEntry* +0x00, members
        // LogicArrayList<AllianceMemberEntry*>* +0x08, description String* +0x10.
        constexpr std::size_t kSize = 0x28;
        constexpr int kHeaderOffset = 0x00;
        constexpr int kMembersOffset = 0x08;
        constexpr int kDescriptionOffset = 0x10;

        const auto entry = library::allocate(kSize);
        library::call("__ZN17AllianceFullEntryC1Ev", {entry});
        library::writePointer(entry + kHeaderOffset, header.createLibraryEntry());
        library::writePointer(entry + kDescriptionOffset, library::newString(description));
        const auto memberList = library::newArrayList();
        for (const auto &member : members)
            library::addArrayItem("__ZN14LogicArrayListIP19AllianceMemberEntryE3addERKS1_", memberList,
                                  member.createLibraryEntry());
        library::writePointer(entry + kMembersOffset, memberList);
        return entry;
    }
};
} // namespace server
