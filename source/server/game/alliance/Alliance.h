#pragma once
// A clan: its settings, its roster and its stream.
#include "game/utils/Json.h"
#include "logic/message/alliance/AllianceHeaderEntry.h"
#include "logic/message/alliance/stream/StreamEntry.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace server {
struct AllianceMember {
    std::int64_t avatarId = 0;
    std::string name;
    int role = 1;
};

class Alliance {
public:
    // Alliance types.
    static constexpr int kTypeOpen = 1;
    static constexpr int kTypeInviteOnly = 2;
    static constexpr int kTypeClosed = 3;
    static constexpr std::size_t kMaxMembers = 50;
    static constexpr std::size_t kMaxStreamEntries = 100;

    Alliance(std::int64_t id, std::string name) : id_(id), name_(std::move(name)) {}

    std::int64_t getId() const { return id_; }
    const std::string &getName() const { return name_; }
    const std::string &getDescription() const { return description_; }
    int getBadge() const { return badge_; }
    int getType() const { return type_; }
    int getRequiredScore() const { return requiredScore_; }
    int getRegion() const { return region_; }
    void setSettings(const std::string &description, int badge, int type, int requiredScore, int region);
    AllianceHeaderEntry getHeaderEntry(int score) const;

    const std::vector<AllianceMember> &getMembers() const { return members_; }
    AllianceMember *getMember(std::int64_t avatarId);
    void addMember(AllianceMember member) { members_.push_back(std::move(member)); }
    void removeMember(std::int64_t avatarId);
    bool isFull() const { return members_.size() >= kMaxMembers; }

    const std::vector<std::unique_ptr<StreamEntry>> &getStreamEntries() const { return streamEntries_; }
    // Takes the entry, gives it the next id and the current time; the oldest entry goes past kMaxStreamEntries.
    StreamEntry &addStreamEntry(std::unique_ptr<StreamEntry> entry);
    // The entry with the id, of the type; null when there is none.
    template <class T> T *getStreamEntry(std::int64_t id) {
        for (auto &entry : streamEntries_)
            if (entry->getId() == id) return dynamic_cast<T *>(entry.get());
        return nullptr;
    }
    // Storage of an entry loaded as it was saved (its id and time kept).
    void loadStreamEntry(std::unique_ptr<StreamEntry> entry) { streamEntries_.push_back(std::move(entry)); }
    std::int64_t getNextStreamEntryId() const { return nextStreamEntryId_; }
    void setNextStreamEntryId(std::int64_t id) { nextStreamEntryId_ = id; }

    // The clans.json format (the Node server's clan.json).
    static std::unique_ptr<Alliance> loadFromJSON(const json::Value &saved);

private:
    std::int64_t id_;
    std::string name_;
    std::string description_;
    int badge_ = 0;
    int type_ = kTypeOpen;
    int requiredScore_ = 0;
    int region_ = 0;
    std::vector<AllianceMember> members_;
    std::vector<std::unique_ptr<StreamEntry>> streamEntries_;
    std::int64_t nextStreamEntryId_ = 1;
};
} // namespace server
