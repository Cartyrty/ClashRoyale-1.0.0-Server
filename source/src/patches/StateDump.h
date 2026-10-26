#pragma once
// HLE_DUMP_HOME debugging aid: home and avatar encodings at each home checksum (client) or turn (server).
#include <cstdint>

namespace statedump {
void dump(const char *file, std::uint64_t mode, const char *who);
void installClientHook();
} // namespace statedump
