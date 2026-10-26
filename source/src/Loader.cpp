#include "Loader.h"
#include "Hle.h"
#include "Log.h"
#include "Memory.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <unordered_map>

namespace loader {
namespace {
struct Segment {
    std::string name;
    std::uint64_t vmaddr, vmsize, fileoff, filesize;
};
std::vector<std::uint8_t> g_file;
std::vector<Segment> g_segments;
std::vector<Section> g_sections;
std::vector<std::string> g_dylibs;
std::uint64_t g_entry = 0, g_base = 0, g_end = 0;
std::vector<std::pair<std::uint64_t, std::string>> g_symbols; // sorted by address
std::unordered_map<std::string, std::uint64_t> g_defined;    // external and local definitions by name

template <class T> T at(std::size_t offset) {
    T v;
    std::memcpy(&v, g_file.data() + offset, sizeof v);
    return v;
}
std::uint64_t uleb(const std::uint8_t *&p) {
    std::uint64_t result = 0;
    int shift = 0;
    std::uint8_t byte;
    do {
        byte = *p++;
        result |= std::uint64_t(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    return result;
}
std::int64_t sleb(const std::uint8_t *&p) {
    std::int64_t result = 0;
    int shift = 0;
    std::uint8_t byte;
    do {
        byte = *p++;
        result |= std::int64_t(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    if (shift < 64 && (byte & 0x40)) result |= -(std::int64_t(1) << shift);
    return result;
}

struct BindStats {
    int binds = 0;
};
// Runs a bind opcode stream (BIND_OPCODE_*). Lazy streams use DONE between entries, so they run to the end.
void runBinds(std::size_t offset, std::size_t size, bool lazy, bool weak, BindStats &stats) {
    if (!size) return;
    const std::uint8_t *p = g_file.data() + offset, *end = p + size;
    int segment = 0;
    std::uint64_t segOffset = 0;
    std::int64_t addend = 0, ordinal = 0;
    std::string name;
    std::uint8_t flags = 0;
    auto bindAt = [&](std::uint64_t address) {
        std::uint64_t target;
        if (weak) {
            if (flags & 8) return; // BIND_SYMBOL_FLAGS_NON_WEAK_DEFINITION: this image defines it.
            auto found = g_defined.find(name);
            if (found != g_defined.end()) target = found->second;
            else target = hle::resolveImport(name);
        } else if (ordinal == -1 || ordinal == 0) { // flat lookup / self
            auto found = g_defined.find(name);
            target = found != g_defined.end() ? found->second : hle::resolveImport(name);
        } else {
            target = hle::resolveImport(name);
        }
        mem::w64(address, target + std::uint64_t(addend));
        ++stats.binds;
    };
    while (p < end) {
        const std::uint8_t byte = *p++;
        const std::uint8_t opcode = byte & 0xF0, imm = byte & 0x0F;
        switch (opcode) {
        case 0x00: // DONE
            if (!lazy) return;
            break;
        case 0x10: ordinal = imm; break;
        case 0x20: ordinal = std::int64_t(uleb(p)); break;
        case 0x30: ordinal = imm ? std::int64_t(std::int8_t(0xF0 | imm)) : 0; break;
        case 0x40:
            flags = imm;
            name = reinterpret_cast<const char *>(p);
            p += name.size() + 1;
            break;
        case 0x50: break; // SET_TYPE_IMM: always pointers here
        case 0x60: addend = sleb(p); break;
        case 0x70:
            segment = imm;
            segOffset = uleb(p);
            break;
        case 0x80: segOffset += uleb(p); break;
        case 0x90:
            bindAt(g_segments[std::size_t(segment)].vmaddr + segOffset);
            segOffset += 8;
            break;
        case 0xA0:
            bindAt(g_segments[std::size_t(segment)].vmaddr + segOffset);
            segOffset += 8 + uleb(p);
            break;
        case 0xB0:
            bindAt(g_segments[std::size_t(segment)].vmaddr + segOffset);
            segOffset += 8 + std::uint64_t(imm) * 8;
            break;
        case 0xC0: {
            const auto count = uleb(p), skip = uleb(p);
            for (std::uint64_t i = 0; i < count; ++i) {
                bindAt(g_segments[std::size_t(segment)].vmaddr + segOffset);
                segOffset += 8 + skip;
            }
            break;
        }
        default: fatal("bad bind opcode %02x", byte);
        }
    }
}
} // namespace

void load(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) fatal("cannot open %s", path.c_str());
    g_file.assign(std::istreambuf_iterator<char>(in), {});
    if (at<std::uint32_t>(0) != 0xFEEDFACF || at<std::uint32_t>(4) != 0x0100000C) fatal("%s is not an arm64 Mach-O", path.c_str());
    const auto ncmds = at<std::uint32_t>(16);
    std::size_t off = 32;
    std::uint32_t symoff = 0, nsyms = 0, stroff = 0;
    std::uint32_t rebaseOff = 0, rebaseSize = 0, bindOff = 0, bindSize = 0, weakOff = 0, weakSize = 0, lazyOff = 0, lazySize = 0;
    for (std::uint32_t i = 0; i < ncmds; ++i) {
        const auto cmd = at<std::uint32_t>(off), size = at<std::uint32_t>(off + 4);
        switch (cmd) {
        case 0x19: { // LC_SEGMENT_64
            Segment s;
            s.name = std::string(reinterpret_cast<const char *>(&g_file[off + 8]), strnlen(reinterpret_cast<const char *>(&g_file[off + 8]), 16));
            s.vmaddr = at<std::uint64_t>(off + 24);
            s.vmsize = at<std::uint64_t>(off + 32);
            s.fileoff = at<std::uint64_t>(off + 40);
            s.filesize = at<std::uint64_t>(off + 48);
            const auto nsects = at<std::uint32_t>(off + 64);
            for (std::uint32_t k = 0; k < nsects; ++k) {
                const std::size_t so = off + 72 + 80 * k;
                Section sec;
                sec.name = std::string(reinterpret_cast<const char *>(&g_file[so]), strnlen(reinterpret_cast<const char *>(&g_file[so]), 16));
                sec.segment = std::string(reinterpret_cast<const char *>(&g_file[so + 16]), strnlen(reinterpret_cast<const char *>(&g_file[so + 16]), 16));
                sec.address = at<std::uint64_t>(so + 32);
                sec.size = at<std::uint64_t>(so + 40);
                g_sections.push_back(sec);
            }
            g_segments.push_back(s);
            break;
        }
        case 0x02: // LC_SYMTAB
            symoff = at<std::uint32_t>(off + 8);
            nsyms = at<std::uint32_t>(off + 12);
            stroff = at<std::uint32_t>(off + 16);
            break;
        case 0x22: case 0x80000022: // LC_DYLD_INFO(_ONLY)
            rebaseOff = at<std::uint32_t>(off + 8); rebaseSize = at<std::uint32_t>(off + 12);
            bindOff = at<std::uint32_t>(off + 16); bindSize = at<std::uint32_t>(off + 20);
            weakOff = at<std::uint32_t>(off + 24); weakSize = at<std::uint32_t>(off + 28);
            lazyOff = at<std::uint32_t>(off + 32); lazySize = at<std::uint32_t>(off + 36);
            break;
        case 0x0C: case 0x80000018: // LC_LOAD_DYLIB / LC_LOAD_WEAK_DYLIB
            g_dylibs.push_back(reinterpret_cast<const char *>(&g_file[off + at<std::uint32_t>(off + 8)]));
            break;
        case 0x80000028: // LC_MAIN
            g_entry = at<std::uint64_t>(off + 8);
            break;
        }
        off += size;
    }
    (void)rebaseOff; (void)rebaseSize; // loaded at the preferred address: slide 0
    // Map the segments.
    for (const auto &s : g_segments) {
        if (s.name == "__PAGEZERO" || !s.vmsize) continue;
        mem::commit(s.vmaddr, s.vmsize);
        std::memcpy(mem::base() + s.vmaddr, g_file.data() + s.fileoff, std::size_t(s.filesize));
        if (s.name == "__TEXT") { g_base = s.vmaddr; g_entry += s.vmaddr; }
        g_end = std::max(g_end, s.vmaddr + s.vmsize);
        hlog("macho", "%-12s %llx-%llx", s.name.c_str(), (unsigned long long)s.vmaddr, (unsigned long long)(s.vmaddr + s.vmsize));
    }
    // Symbols (for weak binds, lookups and crash descriptions).
    for (std::uint32_t i = 0; i < nsyms; ++i) {
        const std::size_t so = symoff + 16 * std::size_t(i);
        const auto strx = at<std::uint32_t>(so);
        const auto type = g_file[so + 4];
        const auto value = at<std::uint64_t>(so + 8);
        if ((type & 0xE0) || (type & 0x0E) != 0x0E || !value) continue; // N_SECT definitions only, no stabs
        std::string name = reinterpret_cast<const char *>(&g_file[stroff + strx]);
        g_symbols.push_back({value, name});
        g_defined.emplace(name, value);
    }
    std::sort(g_symbols.begin(), g_symbols.end());
    hlog("macho", "%zu defined symbols, %zu dylibs, entry %s", g_symbols.size(), g_dylibs.size(), describe(g_entry).c_str());
    BindStats stats;
    runBinds(bindOff, bindSize, false, false, stats);
    runBinds(lazyOff, lazySize, true, false, stats);
    runBinds(weakOff, weakSize, false, true, stats);
    hlog("macho", "%d binds applied", stats.binds);
}
std::uint64_t entryPoint() { return g_entry; }
std::vector<std::uint64_t> initializers() {
    std::vector<std::uint64_t> out;
    if (auto *s = section("__DATA", "__mod_init_func"))
        for (std::uint64_t a = s->address; a < s->address + s->size; a += 8) out.push_back(mem::r64(a));
    return out;
}
const Section *section(const std::string &segment, const std::string &name) {
    for (const auto &s : g_sections) if (s.segment == segment && s.name == name) return &s;
    return nullptr;
}
std::uint64_t symbol(const std::string &name) {
    auto found = g_defined.find(name);
    return found == g_defined.end() ? 0 : found->second;
}
std::uint64_t imageBase() { return g_base; }
std::uint64_t imageEnd() { return g_end; }
std::string describe(std::uint64_t address) {
    char buffer[512];
    if (const char *stub = hle::stubName(address)) {
        std::snprintf(buffer, sizeof buffer, "%s [hle] (0x%llx)", stub, (unsigned long long)address);
        return buffer;
    }
    if (address >= g_base && address < g_end && !g_symbols.empty()) {
        auto it = std::upper_bound(g_symbols.begin(), g_symbols.end(), std::make_pair(address, std::string("\xff")));
        if (it != g_symbols.begin()) {
            --it;
            std::snprintf(buffer, sizeof buffer, "%s+0x%llx (0x%llx)", it->second.c_str(), (unsigned long long)(address - it->first),
                          (unsigned long long)address);
            return buffer;
        }
    }
    std::snprintf(buffer, sizeof buffer, "0x%llx", (unsigned long long)address);
    return buffer;
}
} // namespace loader
