#include "logic/message/avatar/stream/AvatarStreamEntry.h"

#include "game/utils/TimeUtil.h"
#include "logic/data/GlobalID.h"
#include "logic/message/avatar/stream/AllianceKickOutStreamEntry.h"
#include "logic/message/avatar/stream/AllianceMailAvatarStreamEntry.h"
#include "logic/message/avatar/stream/DonationReceivedStreamEntry.h"
#include "logic/message/avatar/stream/JoinAllianceResponseAvatarStreamEntry.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

#include <algorithm>

namespace server {
// Long id, Long sender, String sender name, VInt sender level, VInt age in seconds, Boolean removed, Boolean new
// (AvatarStreamEntry::decode reads the ids with ByteStream::readLong, not as LogicLongs).
void AvatarStreamEntry::encode(ByteStream &stream) const {
    const auto now = TimeUtil::getMilliseconds();
    stream.writeVInt(getAvatarStreamEntryType());
    stream.writeLong(id_);
    stream.writeLong(senderAvatarId_);
    stream.writeStringReference(senderName_);
    stream.writeVInt(senderExpLevel_);
    stream.writeVInt(int(std::max<std::int64_t>(0, (now - creationTime_) / TimeUtil::kMillisecondsPerSecond)));
    stream.writeBoolean(removed_);
    stream.writeBoolean(new_);
    encodeFields(stream);
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

void AvatarStreamEntry::AllianceReference::encode(ByteStream &stream) const {
    stream.writeLong(id);
    stream.writeStringReference(name);
    ByteStreamHelper::writeDataReference(stream, badge);
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
