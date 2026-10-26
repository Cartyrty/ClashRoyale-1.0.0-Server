#pragma once
// Every player's inbox (avatar stream) and the social cooldowns that go with it (card requests, clan mail), saved in
// the avatar_social table as one JSON object per account.
#include "game/utils/Json.h"
#include "logic/message/avatar/stream/AvatarStreamEntry.h"

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace server {
class Database;
class GameServer;
class MessageManager;

class AvatarStreamManager {
public:
    explicit AvatarStreamManager(GameServer &server);

    void load(Database &database);
    // The social parts of the Node server's avatars (its clan.json "avatars").
    void importNodeState(const json::Value &avatars);
    // Replaces the avatar_social rows; inside the caller's transaction.
    void saveRows(Database &database) const;
    void save(); // in a transaction of its own

    // A new entry for the account; an online player gets it at once.
    void addEntry(std::int64_t accountId, std::unique_ptr<AvatarStreamEntry> entry);
    // AvatarStreamMessage with every entry; they are not new any more.
    void sendAvatarStream(MessageManager &player);

    // Cooldown ends (unix seconds).
    std::int64_t getRequestEnd(std::int64_t accountId) const;
    void setRequestEnd(std::int64_t accountId, std::int64_t seconds);
    std::int64_t getTeamMailEnd(std::int64_t accountId) const;
    void setTeamMailEnd(std::int64_t accountId, std::int64_t seconds);

private:
    struct AvatarSocialState {
        std::vector<std::unique_ptr<AvatarStreamEntry>> entries;
        std::int64_t nextEntryId = 1;
        std::int64_t requestEnd = 0;
        std::int64_t teamMailEnd = 0;
    };
    static AvatarSocialState loadState(const json::Value &saved);
    static json::Value saveState(const AvatarSocialState &state);

    GameServer &server_;
    std::map<std::int64_t, AvatarSocialState> states_;
};
} // namespace server
