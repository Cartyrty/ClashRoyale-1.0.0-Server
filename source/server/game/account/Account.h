#pragma once
// A player's account: login data and the saved home and avatar (their own library encodings), as stored in the
// accounts table.
#include <cstdint>
#include <string>
#include <vector>

namespace server {
struct Account {
    std::int64_t id = 0;
    std::string passToken;
    std::vector<std::uint8_t> home;   // LogicClientHome::encode
    std::vector<std::uint8_t> avatar; // LogicClientAvatar::encode
    std::int64_t savedAt = 0;         // unix seconds of the last save (offline progress on the next load)
    std::int64_t createdMs = 0;
    int sessionCount = 0;
    int playSeconds = 0;
};
} // namespace server
