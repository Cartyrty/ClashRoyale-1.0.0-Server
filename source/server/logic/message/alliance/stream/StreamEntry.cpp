#include "logic/message/alliance/stream/StreamEntry.h"

#include "game/utils/TimeUtil.h"
#include "logic/message/alliance/stream/AllianceEventStreamEntry.h"
#include "logic/message/alliance/stream/ChallengeStreamEntry.h"
#include "logic/message/alliance/stream/ChatStreamEntry.h"
#include "logic/message/alliance/stream/DonateStreamEntry.h"
#include "logic/message/alliance/stream/JoinRequestAllianceStreamEntry.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

#include <algorithm>

namespace server {
// StreamEntry::encode: LogicLong id, LogicLong sender avatar id, LogicLong sender home id, String sender name, VInt
// sender level, VInt sender role, VInt age in seconds, Boolean removed.
void StreamEntry::encode(ByteStream &stream) const {
    const auto now = TimeUtil::getMilliseconds();
    stream.writeVInt(getStreamEntryType());
    ByteStreamHelper::encodeLogicLong(stream, id_);
    ByteStreamHelper::encodeLogicLong(stream, senderAvatarId_);
    ByteStreamHelper::encodeLogicLong(stream, senderAvatarId_);
    stream.writeStringReference(senderName_);
    stream.writeVInt(senderExpLevel_);
    stream.writeVInt(senderRole_);
    stream.writeVInt(int(std::max<std::int64_t>(0, (now - creationTime_) / TimeUtil::kMillisecondsPerSecond)));
    stream.writeBoolean(removed_);
    encodeFields(stream);
}

json::Value StreamEntry::save() const {
    auto saved = json::Value::object();
    saved["type"] = getStreamEntryType();
    saved["id"] = id_;
    saved["time"] = creationTime_;
    saved["sender"] = senderAvatarId_;
    saved["name"] = senderName_;
    saved["role"] = senderRole_;
    saved["removed"] = removed_;
    saveFields(saved);
    return saved;
}

std::unique_ptr<StreamEntry> StreamEntry::load(const json::Value &saved) {
    std::unique_ptr<StreamEntry> entry;
    // The Node server's chat messages have no type.
    switch (int(saved["type"].integer(kChat))) {
    case kDonate: entry = std::make_unique<DonateStreamEntry>(); break;
    case kChat: entry = std::make_unique<ChatStreamEntry>(); break;
    case kJoinRequest: entry = std::make_unique<JoinRequestAllianceStreamEntry>(); break;
    case kAllianceEvent: entry = std::make_unique<AllianceEventStreamEntry>(); break;
    case kChallenge: entry = std::make_unique<ChallengeStreamEntry>(); break;
    default: return nullptr;
    }
    entry->id_ = saved["id"].integer();
    entry->creationTime_ = saved["time"].integer(TimeUtil::getMilliseconds());
    entry->senderAvatarId_ = saved["sender"].integer();
    entry->senderName_ = saved["name"].str("");
    entry->senderRole_ = int(saved["role"].integer(1));
    entry->removed_ = saved["removed"].boolean();
    entry->loadFields(saved);
    return entry;
}
} // namespace server
