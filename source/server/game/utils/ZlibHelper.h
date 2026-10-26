#pragma once
// ZlibHelper as on the client, the server's half: data for ZlibHelper::decompressInMySQLFormat (replays, compressed
// sector states). Compressed by the library's own zlib.
#include <cstdint>
#include <string>
#include <vector>

namespace server {
namespace ZlibHelper {
// Int32 (little endian) uncompressed length, then a zlib stream.
std::vector<std::uint8_t> compressInMySQLFormat(const std::string &data);
} // namespace ZlibHelper
} // namespace server
