#pragma once
// The player's home and avatar, and the random seed both sides load them with. Encoded by the library's own
// OwnHomeDataMessage.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <vector>

namespace server {
class OwnHomeDataMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 24101;

    void encode() override;
    int getMessageType() const override { return kMessageType; }
    int getMessageVersion() const override { return 10; }

    void setHome(const std::vector<std::uint8_t> &encodedHome) { home_ = encodedHome; }
    void setAvatar(const std::vector<std::uint8_t> &encodedAvatar) { avatar_ = encodedAvatar; }
    void setRandomSeed(int seed) { randomSeed_ = seed; }

    // Splits a whole payload (an exported account) into the home's and the avatar's encodings.
    static bool decodeHomeAndAvatar(const std::vector<std::uint8_t> &payload, std::vector<std::uint8_t> &home,
                                    std::vector<std::uint8_t> &avatar);

private:
    std::vector<std::uint8_t> home_;
    std::vector<std::uint8_t> avatar_;
    int randomSeed_ = 0;
};
} // namespace server
