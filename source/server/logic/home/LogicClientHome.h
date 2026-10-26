#pragma once
// The library's LogicClientHome (decks, chests, shop, tutorials). Saved as its own encoding (LogicClientHome::encode).
// A LogicClientHome either owns its library object (decode) or only views one that a LogicGameMode owns (view).
#include "library/Library.h"

#include <cstdint>
#include <vector>

namespace server {
class ByteStream;

class LogicClientHome {
public:
    static LogicClientHome decode(const std::vector<std::uint8_t> &encoded);
    static LogicClientHome view(library::Ptr home);

    LogicClientHome(LogicClientHome &&other) noexcept;
    LogicClientHome &operator=(LogicClientHome &&other) noexcept;
    LogicClientHome(const LogicClientHome &) = delete;
    LogicClientHome &operator=(const LogicClientHome &) = delete;
    ~LogicClientHome();

    std::vector<std::uint8_t> encode() const;
    void encode(ByteStream &stream) const; // into a stream other fields share
    LogicClientHome clone() const;         // an owned copy, through its encoding

    void setHomeId(std::int64_t id);
    int getDeckCount() const;
    void addEmptyDeck();
    library::Ptr getSelectedDeck() const; // LogicSpellDeck*
    void destroyChests();
    void debugFinishTutorial(); // every home tutorial done (reaches the avatar through the home's game mode)

    // The daily shop: setShopValues rolls the cards when the seed or the weekday changes.
    int getShopSeed() const;
    int getShopWeekday() const;
    void setShopValues(int seed, int secondsLeft, int weekday);

    int getChestCount() const;
    int getMaxChestCount() const;
    void addChest(library::Ptr chest); // the home owns the chest
    int getSpellCount(library::Ptr spellData) const;

    library::Ptr get() const { return home_; }
    library::Ptr release(); // for a LogicGameMode that takes the home

private:
    LogicClientHome(library::Ptr home, bool owned) : home_(home), owned_(owned) {}
    void destroy();

    library::Ptr home_ = 0;
    bool owned_ = false;
};
} // namespace server
