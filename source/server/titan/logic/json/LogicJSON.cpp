#include "titan/logic/json/LogicJSON.h"

namespace server {
namespace LogicJSON {
using library::call;
using library::Ptr;

namespace {
// StringBuilder (0x20 bytes): length + 1 at +0, characters at +8.
constexpr std::size_t kStringBuilderSize = 0x20;
constexpr int kStringBuilderCharactersOffset = 8;
constexpr std::size_t kJSONObjectSize = 0x28;

std::string writeToString(const char *writeFunction, Ptr node) {
    const auto builder = library::allocate(kStringBuilderSize);
    call("__ZN13StringBuilderC1Ev", {builder});
    call(writeFunction, {node, builder});
    const int length = library::readInt(builder) - 1;
    const auto characters = library::readPointer(builder + kStringBuilderCharactersOffset);
    const auto bytes =
        characters && length > 0 ? library::readBytes(characters, std::size_t(length)) : std::vector<std::uint8_t>();
    call("__ZN13StringBuilderD1Ev", {builder});
    return std::string(bytes.begin(), bytes.end());
}
} // namespace

Ptr createObject() {
    const auto object = library::allocate(kJSONObjectSize);
    call("__ZN15LogicJSONObjectC1Ev", {object});
    return object;
}

std::string writeObject(Ptr object) {
    return object ? writeToString("__ZNK15LogicJSONObject13writeToStringEP13StringBuilder", object) : "{}";
}

std::string writeArray(Ptr array) {
    return array ? writeToString("__ZNK14LogicJSONArray13writeToStringEP13StringBuilder", array) : "[]";
}
} // namespace LogicJSON
} // namespace server
