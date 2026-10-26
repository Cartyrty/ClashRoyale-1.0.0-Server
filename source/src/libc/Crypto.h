#pragma once
// Host-side hashing shared by the emulator and the server (the same code as the CommonCrypto HLE functions).
#include <cstddef>
#include <string>

namespace crypto {
std::string sha1Hex(const void *data, std::size_t size); // lowercase hex
} // namespace crypto
