#pragma once
// An entry of a clan's stream (chat, donation requests, join requests, events, challenges), as StreamEntry is on the
// client. Sent as the library's own entry (StreamEntryFactory::createStreamEntryByType, the server's values put in its
// fields), which the library's stream messages encode; saved as JSON in the clan_entries table.
#include "game/utils/Json.h"
#include "library/Library.h"

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

    // A new library StreamEntry of this type; its owner (a stream message's list) frees it.
    library::Ptr createLibraryEntry() const;
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
    // The subclass's own fields of the library entry.
    virtual void fillLibraryEntry(library::Ptr entry) const = 0;
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
