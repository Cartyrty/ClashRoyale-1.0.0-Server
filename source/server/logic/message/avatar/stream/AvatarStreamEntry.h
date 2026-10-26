#pragma once
// An entry of a player's inbox (avatar stream): donations received, join request answers, kicks, clan mail. Encoded as
// AvatarStreamEntryFactory reads it (VInt type, the common fields, the subclass's fields) and saved as JSON in the
// avatar_social table.
#include "game/utils/Json.h"
#include "titan/logic/datastream/ByteStream.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace server {
class AvatarStreamEntry {
public:
    // AvatarStreamEntryFactory types.
    static constexpr int kBattleReport = 2;
    static constexpr int kJoinAllianceResponse = 3;
    static constexpr int kAllianceKickOut = 5;
    static constexpr int kAllianceMail = 6;
    static constexpr int kDonationReceived = 7;

    virtual ~AvatarStreamEntry() = default;
    virtual int getAvatarStreamEntryType() const = 0;

    void encode(ByteStream &stream) const;
    json::Value save() const;
    static std::unique_ptr<AvatarStreamEntry> load(const json::Value &saved); // null for an unknown type

    void setId(std::int64_t id) { id_ = id; }
    void setSender(std::int64_t avatarId, const std::string &name, int expLevel) {
        senderAvatarId_ = avatarId;
        senderName_ = name;
        senderExpLevel_ = expLevel;
    }
    void setCreationTime(std::int64_t milliseconds) { creationTime_ = milliseconds; }
    void setNew(bool isNew) { new_ = isNew; }

protected:
    virtual void encodeFields(ByteStream &stream) const = 0;
    virtual void saveFields(json::Value &saved) const = 0;
    virtual void loadFields(const json::Value &saved) = 0;

    // The alliance an entry is about: Long id, String name, badge.
    struct AllianceReference {
        std::int64_t id = 0;
        std::string name;
        int badge = 0;

        void encode(ByteStream &stream) const;
        void save(json::Value &saved) const;
        void load(const json::Value &saved);
    };
    // A message that may be missing (a null String; saved only when present).
    static void saveOptional(json::Value &saved, const char *key, const std::optional<std::string> &value);
    static std::optional<std::string> loadOptional(const json::Value &saved, const char *key);

private:
    std::int64_t id_ = 0;
    std::int64_t senderAvatarId_ = 0;
    std::string senderName_;
    int senderExpLevel_ = 1;
    std::int64_t creationTime_ = 0;
    bool removed_ = false;
    bool new_ = true;
};
} // namespace server
