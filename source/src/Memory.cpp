#include "Memory.h"
#include "Log.h"
#include "platform/Platform.h"
#include <map>
#include <mutex>
#include <vector>

namespace mem {
namespace {
std::uint8_t *g_base = nullptr;
std::mutex g_commitLock;
std::uint64_t g_hleTop = kHleBase;
std::mutex g_hleLock;
std::uint64_t g_stackTop = kStackBase;
std::mutex g_stackLock;

// Heap: power-of-two size classes up to 1 MB with free lists; larger blocks are page multiples kept in a size map.
// Each block has a 16-byte header {u64 requested size, u64 class (or 0xFFFF for large) | block size << 16}.
constexpr int kClasses = 17; // 16 B .. 1 MB (payload sizes)
std::mutex g_heapLock;
std::uint64_t g_heapTop = kHeapBase, g_heapCommitted = kHeapBase;
std::vector<std::uint64_t> g_free[kClasses];
std::multimap<std::uint64_t, std::uint64_t> g_largeFree; // block size -> block address
int sizeClass(std::size_t size) {
    int c = 0;
    std::size_t s = 16;
    while (s < size) { s <<= 1; ++c; }
    return c;
}
std::uint64_t bump(std::uint64_t size) {
    const std::uint64_t block = g_heapTop;
    g_heapTop += size;
    if (g_heapTop > kHeapBase + kHeapSize) fatal("guest heap exhausted");
    if (g_heapTop > g_heapCommitted) {
        const std::uint64_t grow = ((g_heapTop - g_heapCommitted) + 0x3FFFFFF) & ~0x3FFFFFFull; // 64 MB steps
        commit(g_heapCommitted, grow);
        g_heapCommitted += grow;
    }
    return block;
}
} // namespace

void init() {
    g_base = static_cast<std::uint8_t *>(platform::reserveAddressSpace(kSpaceSize));
    if (!g_base) fatal("could not reserve the 64 GB guest address space");
    commit(kHleBase, 0x100000);
}
std::uint8_t *base() { return g_base; }
void commit(std::uint64_t address, std::uint64_t size) {
    std::lock_guard lock(g_commitLock);
    const std::uint64_t start = address & ~0xFFFull, end = (address + size + 0xFFF) & ~0xFFFull;
    if (!platform::commitMemory(g_base + start, end - start))
        fatal("commit failed at %llx (+%llx)", (unsigned long long)start, (unsigned long long)(end - start));
}
bool valid(std::uint64_t address, std::uint64_t size) {
    if (address < 0x1000 || address + size > kSpaceSize) return false;
    return platform::isCommitted(g_base + address);
}
std::string cstring(std::uint64_t address, std::size_t maximum) {
    std::string out;
    if (!address) return out;
    for (std::size_t i = 0; i < maximum; ++i) {
        const char c = static_cast<char>(g_base[address + i]);
        if (!c) break;
        out.push_back(c);
    }
    return out;
}
std::uint64_t guestAddress(const void *host) { return std::uint64_t(static_cast<const std::uint8_t *>(host) - g_base); }

std::uint64_t hleAlloc(std::size_t size, std::size_t align) {
    std::lock_guard lock(g_hleLock);
    g_hleTop = (g_hleTop + align - 1) & ~(std::uint64_t(align) - 1);
    const std::uint64_t address = g_hleTop;
    g_hleTop += size;
    static std::uint64_t committed = kHleBase + 0x100000;
    if (g_hleTop > committed) {
        const std::uint64_t grow = ((g_hleTop - committed) + 0xFFFFF) & ~0xFFFFFull;
        commit(committed, grow);
        committed += grow;
    }
    std::memset(g_base + address, 0, size);
    return address;
}
std::uint64_t hleString(std::string_view text) {
    const auto address = hleAlloc(text.size() + 1, 1);
    std::memcpy(g_base + address, text.data(), text.size());
    return address;
}

std::uint64_t alloc(std::size_t size) {
    std::lock_guard lock(g_heapLock);
    if (size <= (1u << 20)) {
        const int c = sizeClass(size ? size : 1);
        std::uint64_t block;
        if (!g_free[c].empty()) { block = g_free[c].back(); g_free[c].pop_back(); }
        else block = bump(16 + (16ull << c));
        w64(block, size);
        w64(block + 8, std::uint64_t(c));
        return block + 16;
    }
    const std::uint64_t blockSize = (size + 16 + 0xFFFF) & ~0xFFFFull;
    std::uint64_t block;
    auto found = g_largeFree.lower_bound(blockSize);
    if (found != g_largeFree.end() && found->first == blockSize) { block = found->second; g_largeFree.erase(found); }
    else block = bump(blockSize);
    w64(block, size);
    w64(block + 8, 0xFFFF | (blockSize << 16));
    return block + 16;
}
std::uint64_t calloc(std::size_t size) {
    const auto address = alloc(size);
    std::memset(g_base + address, 0, size);
    return address;
}
std::size_t allocSize(std::uint64_t address) {
    if (address < kHeapBase || address >= kHeapBase + kHeapSize) return 0;
    const auto tag = r64(address - 8);
    if ((tag & 0xFFFF) == 0xFFFF) return std::size_t((tag >> 16) - 16);
    return std::size_t(16ull << tag);
}
void free(std::uint64_t address) {
    if (!address) return;
    if (address < kHeapBase || address >= g_heapTop) { warn("free of non-heap pointer %llx", (unsigned long long)address); return; }
    std::lock_guard lock(g_heapLock);
    const std::uint64_t block = address - 16;
    const auto tag = r64(block + 8);
    if ((tag & 0xFFFF) == 0xFFFF) g_largeFree.emplace(tag >> 16, block);
    else if (tag < kClasses) g_free[tag].push_back(block);
    else warn("free of corrupt block %llx", (unsigned long long)address);
}
std::uint64_t realloc(std::uint64_t address, std::size_t size) {
    if (!address) return alloc(size);
    const std::size_t capacity = allocSize(address);
    if (size <= capacity) { w64(address - 16, size); return address; }
    const auto moved = alloc(size);
    std::memcpy(g_base + moved, g_base + address, std::min<std::size_t>(capacity, size));
    free(address);
    return moved;
}
std::uint64_t heapString(std::string_view text) {
    const auto address = alloc(text.size() + 1);
    std::memcpy(g_base + address, text.data(), text.size());
    g_base[address + text.size()] = 0;
    return address;
}
std::uint64_t allocStack(std::size_t size) {
    std::lock_guard lock(g_stackLock);
    const std::uint64_t bottom = g_stackTop + 0x10000; // guard gap
    g_stackTop = bottom + size;
    commit(bottom, size);
    return bottom + size; // initial SP (grows down)
}
} // namespace mem
