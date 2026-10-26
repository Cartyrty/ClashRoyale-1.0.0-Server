#pragma once
// The v1 stream cipher (the client's RC4Encrypter): RC4 keyed with the game's key + "nonce", the first key-length
// bytes of keystream discarded. Each direction of a connection has its own encrypter.
#include <cstddef>
#include <cstdint>
#include <string>

namespace server {
class RC4Encrypter {
public:
    static const char *const kKey;

    explicit RC4Encrypter(const std::string &key);
    void encrypt(std::uint8_t *data, std::size_t length); // RC4 is symmetric: also decrypts

private:
    std::uint8_t state_[256];
    std::uint8_t i_ = 0;
    std::uint8_t j_ = 0;
};
} // namespace server
