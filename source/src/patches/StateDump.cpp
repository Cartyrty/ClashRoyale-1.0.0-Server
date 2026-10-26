// HLE_DUMP_HOME=<file>: debugging aid for out-of-sync homes. Appends the home and avatar encodings (LogicClientHome /
// LogicClientAvatar::encode) with the tick each time the library calculates a home checksum, so a client dump and a
// server dump (see stateDump) can be compared byte by byte.
#include "StateDump.h"
#include "Cpu.h"
#include "Hle.h"
#include "Loader.h"
#include "Memory.h"
#include <cstdio>
#include <cstdlib>
#include <string>

namespace statedump {
namespace {
std::uint64_t fn(const char *name) { return loader::symbol(name); }
std::string encodeHex(std::uint64_t object, const char *encodeSymbol) {
    const auto stream = mem::calloc(0x40);
    cpu::call(fn("__ZN10ByteStreamC1Ei"), {stream, 256});
    cpu::call(fn(encodeSymbol), {object, stream});
    const auto length = int(cpu::call(fn("__ZNK10ByteStream9getLengthEv"), {stream}));
    const auto data = cpu::call(fn("__ZNK10ByteStream12getByteArrayEv"), {stream});
    std::string hex;
    static const char *digits = "0123456789abcdef";
    for (int i = 0; data && i < length; ++i) {
        const auto b = mem::base()[data + std::uint64_t(i)];
        hex += digits[b >> 4];
        hex += digits[b & 15];
    }
    cpu::call(fn("__ZN10ByteStream8destructEv"), {stream});
    mem::free(stream);
    return hex;
}
} // namespace

void dump(const char *file, std::uint64_t mode, const char *who) {
    if (!file || !mode || !mem::r64(mode + 0x50)) return; // homes only (+0x50 is the home)
    const auto home = cpu::call(fn("__ZNK13LogicGameMode7getHomeEv"), {mode});
    const auto avatar = cpu::call(fn("__ZNK13LogicGameMode15getPlayerAvatarEv"), {mode});
    if (!home || !avatar) return;
    const int tick = int(cpu::call(fn("__ZNK13LogicGameMode7getTickEv"), {mode}));
    const auto homeHex = encodeHex(home, "__ZNK15LogicClientHome6encodeEP15ChecksumEncoder");
    const auto avatarHex = encodeHex(avatar, "__ZNK17LogicClientAvatar6encodeEP15ChecksumEncoder");
    if (FILE *f = std::fopen(file, "a")) {
        std::fprintf(f, "%s tick=%d\nhome %s\navatar %s\n", who, tick, homeHex.c_str(), avatarHex.c_str());
        std::fclose(f);
    }
}

void installClientHook() {
    static const char *file = std::getenv("HLE_DUMP_HOME");
    if (!file) return;
    hle::hookFunction("__ZNK13LogicGameMode17calculateChecksumEv", [] { dump(file, hle::arg(0), "client"); });
}
} // namespace statedump
