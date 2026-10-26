#pragma once
// The library's LogicClientAvatar (name, level, trophies, resources, alliance). Saved as its own encoding. Like
// LogicClientHome, it either owns its library object (create, decode) or views one a LogicGameMode owns (view).
#include "library/Library.h"

#include <cstdint>
#include <string>
#include <vector>

namespace server {
class ByteStream;

class LogicClientAvatar {
public:
    static LogicClientAvatar create(); // the constructor's defaults
    static LogicClientAvatar decode(const std::vector<std::uint8_t> &encoded);
    static LogicClientAvatar view(library::Ptr avatar);

    LogicClientAvatar(LogicClientAvatar &&other) noexcept;
    LogicClientAvatar &operator=(LogicClientAvatar &&other) noexcept;
    LogicClientAvatar(const LogicClientAvatar &) = delete;
    LogicClientAvatar &operator=(const LogicClientAvatar &) = delete;
    ~LogicClientAvatar();

    std::vector<std::uint8_t> encode() const;
    void encode(ByteStream &stream) const; // into a stream other fields share

    void setIds(std::int64_t id); // avatar, account and home id
    std::int64_t getAccountId() const;
    std::string getName() const;
    void setName(const std::string &name);
    bool getNameSetByUser() const;
    int getNameChangeState() const;
    int getExpLevel() const;
    void setExpLevel(int level);
    int getExpPoints() const;
    void setExpPoints(int points);
    int getScore() const; // trophies
    void setScore(int score);
    int getGold() const;
    void setCommodityCount(int commodityType, library::Ptr data, int count);
    void setDiamonds(int diamonds);
    void setFreeDiamonds(int diamonds);
    library::Ptr getArena() const; // LogicArenaData*
    void setArena(library::Ptr arenaData);
    std::string getAllianceName() const;
    library::Ptr getAllianceBadge() const; // LogicAllianceBadgeData*
    library::Ptr getNextWinnerChest(library::Ptr arenaData) const;
    void updateWinnerChestIndex();

    library::Ptr get() const { return avatar_; }
    library::Ptr release(); // for a LogicGameMode that takes the avatar

private:
    LogicClientAvatar(library::Ptr avatar, bool owned) : avatar_(avatar), owned_(owned) {}
    void destroy();

    library::Ptr avatar_ = 0;
    bool owned_ = false;
};
} // namespace server
