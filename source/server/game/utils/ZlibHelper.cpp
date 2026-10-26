#include "game/utils/ZlibHelper.h"

#include "library/Library.h"

namespace server {
namespace ZlibHelper {
namespace {
// zlib's z_stream on arm64 (112 bytes): next_in +0, avail_in +8, next_out +24, avail_out +32, total_out +40; zalloc,
// zfree and opaque null (zlib's own allocators).
constexpr std::size_t kZStreamSize = 112;
constexpr int kNextInOffset = 0;
constexpr int kAvailInOffset = 8;
constexpr int kNextOutOffset = 24;
constexpr int kAvailOutOffset = 32;
constexpr int kTotalOutOffset = 40;
constexpr int kBestCompression = 9;
constexpr int kDeflated = 8;
constexpr int kWindowBits = 15; // a zlib stream (header and adler32), as ZlibHelper::decompressInMySQLFormat inflates
constexpr int kMemoryLevel = 8;
constexpr int kDefaultStrategy = 0;
constexpr int kFinish = 4;
constexpr int kStreamEnd = 1;
constexpr const char *kZlibVersion = "1.2.8"; // deflateInit2_ checks the major version only

std::uint64_t argument(int value) { return std::uint64_t(std::uint32_t(value)); }

void appendLittleEndian32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(std::uint8_t(value >> (8 * i)));
}
} // namespace

// The library's own zlib deflates.
std::vector<std::uint8_t> compressInMySQLFormat(const std::string &data) {
    std::vector<std::uint8_t> out;
    appendLittleEndian32(out, std::uint32_t(data.size()));
    const auto input = library::copyBytes(std::vector<std::uint8_t>(data.begin(), data.end()));
    const std::size_t capacity = data.size() + data.size() / 1000 + 64; // deflateBound for any input
    const auto output = library::allocate(capacity);
    const auto stream = library::allocate(kZStreamSize);
    const auto version = library::copyBytes(std::vector<std::uint8_t>(kZlibVersion, kZlibVersion + 6));
    library::call("_deflateInit2_", {stream, argument(kBestCompression), argument(kDeflated), argument(kWindowBits),
                                     argument(kMemoryLevel), argument(kDefaultStrategy), version, kZStreamSize});
    library::writePointer(stream + kNextInOffset, input);
    library::writeInt(stream + kAvailInOffset, std::int32_t(data.size()));
    library::writePointer(stream + kNextOutOffset, output);
    library::writeInt(stream + kAvailOutOffset, std::int32_t(capacity));
    const int result = int(library::call("_deflate", {stream, argument(kFinish)}));
    const auto length = std::size_t(library::readPointer(stream + kTotalOutOffset));
    library::call("_deflateEnd", {stream});
    if (result == kStreamEnd) {
        const auto compressed = library::readBytes(output, length);
        out.insert(out.end(), compressed.begin(), compressed.end());
    }
    for (const auto memory : {input, output, stream, version}) library::release(memory);
    return out;
}
} // namespace ZlibHelper
} // namespace server
