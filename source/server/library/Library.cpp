#include "library/Library.h"

#include "Cpu.h"
#include "Loader.h"
#include "Memory.h"
#include "titan/logic/debug/Debugger.h"

#include <cstring>
#include <string>
#include <unordered_map>

namespace library {
Ptr symbol(const char *mangledName) {
    static std::unordered_map<std::string, Ptr> cache;
    if (const auto found = cache.find(mangledName); found != cache.end()) return found->second;
    const auto address = loader::symbol(mangledName);
    if (!address) server::Debugger::error("library symbol %s not found", mangledName);
    cache.emplace(mangledName, address);
    return address;
}

Ptr call(const char *mangledName, std::initializer_list<std::uint64_t> arguments) {
    return cpu::call(symbol(mangledName), arguments);
}

Ptr virtualCall(Ptr object, int vtableOffset, std::initializer_list<std::uint64_t> extraArguments) {
    std::vector<std::uint64_t> arguments{object};
    arguments.insert(arguments.end(), extraArguments.begin(), extraArguments.end());
    const auto function = mem::r64(mem::r64(object) + std::uint64_t(vtableOffset));
    switch (arguments.size()) {
    case 1: return cpu::call(function, {arguments[0]});
    case 2: return cpu::call(function, {arguments[0], arguments[1]});
    case 3: return cpu::call(function, {arguments[0], arguments[1], arguments[2]});
    case 4: return cpu::call(function, {arguments[0], arguments[1], arguments[2], arguments[3]});
    default: return cpu::call(function, {arguments[0], arguments[1], arguments[2], arguments[3], arguments[4]});
    }
}

void deleteObject(Ptr object) {
    if (object) virtualCall(object, vtable::kDeletingDestructor);
}

Ptr allocate(std::size_t size) { return mem::calloc(size); }
void release(Ptr memory) {
    if (memory) mem::free(memory);
}
Ptr copyBytes(const std::vector<std::uint8_t> &bytes) {
    const auto buffer = mem::calloc(bytes.size() + 1);
    if (!bytes.empty()) std::memcpy(mem::base() + buffer, bytes.data(), bytes.size());
    return buffer;
}
std::vector<std::uint8_t> readBytes(Ptr address, std::size_t length) {
    return std::vector<std::uint8_t>(mem::base() + address, mem::base() + address + length);
}
std::int32_t readInt(Ptr address) { return std::int32_t(mem::r32(address)); }
void writeInt(Ptr address, std::int32_t value) { mem::w32(address, std::uint32_t(value)); }
Ptr readPointer(Ptr address) { return mem::r64(address); }
void writePointer(Ptr address, Ptr value) { mem::w64(address, value); }

Ptr newString(const std::string &text) {
    const auto characters = mem::heapString(text);
    const auto string = allocate(0x20);
    call("__ZN6StringC1EPKc", {string, characters});
    mem::free(characters);
    return string;
}

std::string readString(Ptr string) {
    if (!string) return {};
    const auto byteCount = mem::r32(string + 4);
    const auto characters = byteCount + 1 > 8 ? mem::r64(string + 8) : string + 8;
    if (!characters) return {};
    return std::string(reinterpret_cast<const char *>(mem::base() + characters), byteCount);
}

std::string readStringPointer(Ptr stringPointerField) {
    return stringPointerField ? readString(stringPointerField) : std::string();
}

Ptr newLogicLong(std::int64_t value) {
    const auto logicLong = allocate(8);
    writeLogicLong(logicLong, value);
    return logicLong;
}

std::int64_t readLogicLong(Ptr logicLong) {
    if (!logicLong) return 0;
    return (std::int64_t(std::int32_t(mem::r32(logicLong))) << 32) | mem::r32(logicLong + 4);
}

void writeLogicLong(Ptr address, std::int64_t value) {
    mem::w32(address, std::uint32_t(value >> 32));
    mem::w32(address + 4, std::uint32_t(value));
}

int arrayCount(Ptr list) { return list ? int(mem::r32(list + 0xc)) : 0; }
Ptr arrayItem(Ptr list, int index) { return mem::r64(mem::r64(list) + 8 * std::uint64_t(index)); }
void setArrayItem(Ptr list, int index, Ptr value) { mem::w64(mem::r64(list) + 8 * std::uint64_t(index), value); }
std::int32_t arrayIntItem(Ptr list, int index) {
    return std::int32_t(mem::r32(mem::r64(list) + 4 * std::uint64_t(index)));
}
} // namespace library
