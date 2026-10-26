#pragma once
// A roster event (joined, left, kicked, promoted, ...): the entry's sender is the member it happened to, the target
// the member who did it.
#include "logic/message/alliance/stream/StreamEntry.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

#include <cstdint>
#include <string>

namespace server {
class AllianceEventStreamEntry : public StreamEntry {
public:
    // AllianceEventStreamEntry event types.
    static constexpr int kKicked = 1;
    static constexpr int kAccepted = 2;
    static constexpr int kJoined = 3;
    static constexpr int kLeft = 4;
    static constexpr int kPromoted = 5;
    static constexpr int kDemoted = 6;

    int getStreamEntryType() const override { return kAllianceEvent; }

    void setEventType(int eventType) { eventType_ = eventType; }
    void setTarget(std::int64_t avatarId, const std::string &name) {
        targetAvatarId_ = avatarId;
        targetName_ = name;
    }

protected:
    void encodeFields(ByteStream &stream) const override {
        stream.writeVInt(eventType_);
        ByteStreamHelper::encodeLogicLong(stream, targetAvatarId_);
        stream.writeStringReference(targetName_);
    }
    void saveFields(json::Value &saved) const override {
        saved["eventType"] = eventType_;
        saved["target"] = targetAvatarId_;
        saved["targetName"] = targetName_;
    }
    void loadFields(const json::Value &saved) override {
        eventType_ = int(saved["eventType"].integer());
        targetAvatarId_ = saved["target"].integer();
        targetName_ = saved["targetName"].str("");
    }

private:
    int eventType_ = 0;
    std::int64_t targetAvatarId_ = 0;
    std::string targetName_;
};
} // namespace server
