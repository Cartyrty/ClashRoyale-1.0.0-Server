#pragma once
// Guest address space: one 64 GB host reservation that dynarmic accesses directly (fastmem). Guest address A lives at
// base() + A. Layout:
//   0x0_0000_0000  null page (never committed)
//   0x1_0000_0000  the app's Mach-O segments
//   0x2_0000_0000  HLE region: import trap stubs, HLE data symbols, constant objects
//   0x3_0000_0000  guest heap (malloc / objc objects / CF objects)
//   0xE_0000_0000  thread stacks
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace mem {
constexpr std::uint64_t kAddressBits = 36;
constexpr std::uint64_t kSpaceSize = 1ull << kAddressBits;
constexpr std::uint64_t kHleBase = 0x200000000ull, kHleSize = 0x10000000ull;
constexpr std::uint64_t kHeapBase = 0x300000000ull, kHeapSize = 0x400000000ull;
constexpr std::uint64_t kStackBase = 0xE00000000ull, kStackSize = 0x100000000ull;

void init();
std::uint8_t *base();
void commit(std::uint64_t address, std::uint64_t size);
bool valid(std::uint64_t address, std::uint64_t size = 1);

template <class T> inline T *ptr(std::uint64_t address) { return reinterpret_cast<T *>(base() + address); }
template <class T> inline T read(std::uint64_t address) { T value; std::memcpy(&value, base() + address, sizeof(T)); return value; }
template <class T> inline void write(std::uint64_t address, T value) { std::memcpy(base() + address, &value, sizeof(T)); }
inline std::uint64_t r64(std::uint64_t a) { return read<std::uint64_t>(a); }
inline std::uint32_t r32(std::uint64_t a) { return read<std::uint32_t>(a); }
inline void w64(std::uint64_t a, std::uint64_t v) { write(a, v); }
inline void w32(std::uint64_t a, std::uint32_t v) { write(a, v); }
std::string cstring(std::uint64_t address, std::size_t maximum = 1 << 20);
std::uint64_t guestAddress(const void *host);

// Bump allocator for the HLE region (stubs, data symbols, permanent objects). Never freed.
std::uint64_t hleAlloc(std::size_t size, std::size_t align = 16);
std::uint64_t hleString(std::string_view text);

// The guest heap (thread safe). Blocks are 16-byte aligned and zero-filled when calloc is used.
std::uint64_t alloc(std::size_t size);
std::uint64_t calloc(std::size_t size);
std::uint64_t realloc(std::uint64_t address, std::size_t size);
void free(std::uint64_t address);
std::size_t allocSize(std::uint64_t address);
std::uint64_t heapString(std::string_view text);

// Thread stacks.
std::uint64_t allocStack(std::size_t size);
} // namespace mem
