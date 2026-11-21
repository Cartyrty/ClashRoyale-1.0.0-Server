#include "logic/message/avatar/stream/AvatarStreamEntry.h"

#include "game/utils/TimeUtil.h"
#include "logic/data/GlobalID.h"
#include "logic/message/avatar/stream/AllianceKickOutStreamEntry.h"
#include "logic/message/avatar/stream/AllianceMailAvatarStreamEntry.h"
#include "logic/message/avatar/stream/DonationReceivedStreamEntry.h"
#include "logic/message/avatar/stream/JoinAllianceResponseAvatarStreamEntry.h"
#include "logic/data/LogicDataTables.h"

#include <algorithm>

namespace server {
// AvatarStreamEntry: id LogicLong* +0x08, sender avatar id LogicLong* +0x10, sender name String* +0x18, sender level
// +0x20, age in seconds +0x24 (setAgeSeconds), removed bool +0x28, new bool +0x29 (setIsNew; true by default).
library::Ptr AvatarStreamEntry::createLibraryEntry() const {
    constexpr int kIdOffset = 0x08;
    constexpr int kSenderAvatarIdOffset = 0x10;
    constexpr int kSenderNameOffset = 0x18;
    constexpr int kSenderExpLevelOffset = 0x20;
    constexpr int kAgeSecondsOffset = 0x24;
    constexpr int kRemovedOffset = 0x28;
    constexpr int kNewOffset = 0x29;
    const auto now = TimeUtil::getMilliseconds();
    const auto entry = library::call("__ZN24AvatarStreamEntryFactory23createStreamEntryByTypeEi",
                                     {std::uint64_t(std::uint32_t(getAvatarStreamEntryType()))});
    library::writePointer(entry + kIdOffset, library::newLogicLong(id_));
    library::writePointer(entry + kSenderAvatarIdOffset, library::newLogicLong(senderAvatarId_));
    library::writePointer(entry + kSenderNameOffset, library::newString(senderName_));
    library::writeInt(entry + kSenderExpLevelOffset, senderExpLevel_);
    library::writeInt(entry + kAgeSecondsOffset,
                      int(std::max<std::int64_t>(0, (now - creationTime_) / TimeUtil::kMillisecondsPerSecond)));
    library::writeByte(entry + kRemovedOffset, removed_ ? 1 : 0);
    library::writeByte(entry + kNewOffset, new_ ? 1 : 0);
    fillLibraryEntry(entry);
    return entry;
}

json::Value AvatarStreamEntry::save() const {
    auto saved = json::Value::object();
    saved["type"] = getAvatarStreamEntryType();
    saved["id"] = id_;
    saved["time"] = creationTime_;
    saved["removed"] = removed_;
    saved["isNew"] = new_;
    saved["sender"] = senderAvatarId_;
    saved["name"] = senderName_;
    saved["level"] = senderExpLevel_;
    saveFields(saved);
    return saved;
}

std::unique_ptr<AvatarStreamEntry> AvatarStreamEntry::load(const json::Value &saved) {
    std::unique_ptr<AvatarStreamEntry> entry;
    switch (int(saved["type"].integer())) {
    case kJoinAllianceResponse: entry = std::make_unique<JoinAllianceResponseAvatarStreamEntry>(); break;
    case kAllianceKickOut: entry = std::make_unique<AllianceKickOutStreamEntry>(); break;
    case kAllianceMail: entry = std::make_unique<AllianceMailAvatarStreamEntry>(); break;
    case kDonationReceived: entry = std::make_unique<DonationReceivedStreamEntry>(); break;
    default: return nullptr;
    }
    entry->id_ = saved["id"].integer();
    entry->creationTime_ = saved["time"].integer(TimeUtil::getMilliseconds());
    entry->removed_ = saved["removed"].boolean();
    entry->new_ = saved["isNew"].boolean(true);
    entry->senderAvatarId_ = saved["sender"].integer();
    entry->senderName_ = saved["name"].str("");
    entry->senderExpLevel_ = int(saved["level"].integer(1));
    entry->loadFields(saved);
    return entry;
}

void AvatarStreamEntry::AllianceReference::fill(library::Ptr entry, int idOffset, int nameOffset,
                                                int badgeOffset) const {
    library::writePointer(entry + std::uint64_t(idOffset), library::newLogicLong(id));
    library::writePointer(entry + std::uint64_t(nameOffset), library::newString(name));
    library::writePointer(entry + std::uint64_t(badgeOffset), LogicDataTables::getDataById(badge));
}

void AvatarStreamEntry::AllianceReference::save(json::Value &saved) const {
    saved["allianceId"] = id;
    saved["allianceName"] = name;
    saved["badge"] = GlobalID::saveToJSON(badge);
}

void AvatarStreamEntry::AllianceReference::load(const json::Value &saved) {
    id = saved["allianceId"].integer();
    name = saved["allianceName"].str("");
    badge = GlobalID::loadFromJSON(saved["badge"]);
}

void AvatarStreamEntry::saveOptional(json::Value &saved, const char *key, const std::optional<std::string> &value) {
    if (value) saved[key] = *value;
}

std::optional<std::string> AvatarStreamEntry::loadOptional(const json::Value &saved, const char *key) {
    if (!saved[key].isString()) return std::nullopt;
    return saved[key].str();
}
} // namespace server
