#pragma once
// An entry of a clan's stream (chat, donation requests, join requests, events, challenges), as StreamEntry is on the
// client. Encoded as StreamEntryFactory reads it (VInt type, StreamEntry::encode, the subclass's fields) and saved as
// JSON in the clan_entries table.
#include "game/utils/Json.h"
#include "titan/logic/datastream/ByteStream.h"

#include <cstdint>
#include <memory>
#include <string>

namespace server {
class StreamEntry {
public:
    // StreamEntryFactory types.
    static constexpr int kDonate = 1;
    static constexpr int kChat = 2;
    static constexpr int kJoinRequest = 3;
    static constexpr int kAllianceEvent = 4;
    static constexpr int kChallenge = 10;

    virtual ~StreamEntry() = default;
    virtual int getStreamEntryType() const = 0;

    void encode(ByteStream &stream) const;
    json::Value save() const;
    // The entry a saved one describes; null for a type the server does not know.
    static std::unique_ptr<StreamEntry> load(const json::Value &saved);

    std::int64_t getId() const { return id_; }
    void setId(std::int64_t id) { id_ = id; }
    std::int64_t getSenderAvatarId() const { return senderAvatarId_; }
    const std::string &getSenderName() const { return senderName_; }
    void setSender(std::int64_t avatarId, const std::string &name, int role) {
        senderAvatarId_ = avatarId;
        senderName_ = name;
        senderRole_ = role;
    }
    // The sender's level when the entry is encoded (not saved: it follows the sender).
    void setSenderExpLevel(int level) { senderExpLevel_ = level; }
    void setCreationTime(std::int64_t milliseconds) { creationTime_ = milliseconds; }
    bool isRemoved() const { return removed_; }
    void setRemoved(bool removed) { removed_ = removed; }

protected:
    virtual void encodeFields(ByteStream &stream) const = 0;
    virtual void saveFields(json::Value &saved) const = 0;
    virtual void loadFields(const json::Value &saved) = 0;

private:
    std::int64_t id_ = 0;
    std::int64_t senderAvatarId_ = 0;
    std::string senderName_;
    int senderRole_ = 1;
    int senderExpLevel_ = 1;
    std::int64_t creationTime_ = 0;
    bool removed_ = false;
};
} // namespace server
