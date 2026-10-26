// libstdc++.6 (the GNU runtime iOS shipped until iOS 12): operator new/delete, guards, atexit, the copy-on-write
// std::string (old ABI), red-black tree helpers, and stubs for the stream classes.
#include "Hle.h"
#include "Loader.h"
#include "Log.h"
#include "Memory.h"
#include "libc/Files.h"
#include "platform/Platform.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

using hle::arg;
using hle::ret;

HLE(_Znwm) { ret(mem::alloc(std::size_t(arg(0)))); }
HLE(_Znam) { ret(mem::alloc(std::size_t(arg(0)))); }
HLE(_ZdlPv) { mem::free(arg(0)); }
HLE(_ZdaPv) { mem::free(arg(0)); }
HLE(_ZnwmRKSt9nothrow_t) { ret(mem::alloc(std::size_t(arg(0)))); }
HLE(_ZnamRKSt9nothrow_t) { ret(mem::alloc(std::size_t(arg(0)))); }
HLE(__cxa_atexit) { ret(0); }
// Function-local statics: guard byte 0 = initialized.
namespace {
std::recursive_mutex g_guardLock;
}
HLE(__cxa_guard_acquire) {
    if (mem::read<std::uint8_t>(arg(0))) return ret(0);
    g_guardLock.lock();
    if (mem::read<std::uint8_t>(arg(0))) { g_guardLock.unlock(); return ret(0); }
    ret(1);
}
HLE(__cxa_guard_release) {
    mem::write<std::uint8_t>(arg(0), 1);
    g_guardLock.unlock();
}
HLE(__cxa_guard_abort) { g_guardLock.unlock(); }
HLE(__cxa_pure_virtual) { cpu::backtrace("pure virtual call"); fatal("__cxa_pure_virtual"); }
HLE(__cxa_demangle) {
    if (arg(3)) mem::w32(arg(3), std::uint32_t(-2));
    ret(0);
}
HLE(__cxa_call_unexpected) { cpu::backtrace("__cxa_call_unexpected"); fatal("__cxa_call_unexpected"); }
HLE(__cxa_rethrow) { cpu::backtrace("C++ rethrow"); fatal("C++ exceptions are not supported (rethrow)"); }
HLE(__cxa_allocate_exception) { ret(mem::calloc(std::size_t(arg(0)) + 128) + 128); }
HLE(__cxa_throw) {
    cpu::backtrace("C++ throw");
    fatal("C++ exception thrown (type info %s)", loader::describe(arg(1)).c_str());
}
HLE(__cxa_begin_catch) { ret(arg(0)); }
HLE(__cxa_end_catch) {}
HLE(_ZSt9terminatev) { cpu::backtrace("std::terminate"); fatal("std::terminate"); }
HLE(_ZSt17__throw_bad_allocv) { cpu::backtrace("bad_alloc"); fatal("std::__throw_bad_alloc"); }
HLE(_ZSt20__throw_length_errorPKc) { cpu::backtrace("length_error"); fatal("std::length_error: %s", mem::cstring(arg(0)).c_str()); }
HLE(_ZSt20__throw_out_of_rangePKc) { cpu::backtrace("out_of_range"); fatal("std::out_of_range: %s", mem::cstring(arg(0)).c_str()); }
HLE(_ZNSt11logic_errorD2Ev) {}
HLE(_ZNSt13runtime_errorD2Ev) {}
HLE(_ZNSt8ios_base4InitC1Ev) {}
HLE(_ZNSt8ios_base4InitD1Ev) {}

// --- std::string (COW): the object holds a pointer to the characters; the _Rep header precedes them:
// {size_t length (-24), size_t capacity (-16), _Atomic_word refcount (-8)}. Refcount 0 = one owner, -1 = leaked.
namespace {
std::uint64_t g_emptyRep = 0;
std::uint64_t emptyRep() {
    if (!g_emptyRep) g_emptyRep = mem::hleAlloc(32, 8);
    return g_emptyRep;
}
std::uint64_t emptyData() { return emptyRep() + 24; }
std::uint64_t newRep(std::uint64_t capacity) {
    const auto rep = mem::alloc(std::size_t(24 + capacity + 1));
    mem::w64(rep, 0);
    mem::w64(rep + 8, capacity);
    mem::w32(rep + 16, 0);
    return rep;
}
std::uint64_t makeData(const std::uint8_t *chars, std::uint64_t n) {
    if (!n) return emptyData();
    const auto rep = newRep(n);
    std::memcpy(mem::base() + rep + 24, chars, std::size_t(n));
    mem::w64(rep, n);
    mem::base()[rep + 24 + n] = 0;
    return rep + 24;
}
void dispose(std::uint64_t data) {
    const auto rep = data - 24;
    if (rep == emptyRep()) return;
    auto *count = reinterpret_cast<std::atomic<std::int32_t> *>(mem::base() + rep + 16);
    if (count->fetch_sub(1) <= 0) mem::free(rep);
}
std::uint64_t length(std::uint64_t data) { return mem::r64(data - 24); }
std::uint64_t capacity(std::uint64_t data) { return mem::r64(data - 16); }
// Gives the string at `self` a private buffer of at least `cap` bytes, preserving the first `keep` characters.
std::uint64_t reserveUnique(std::uint64_t self, std::uint64_t cap, std::uint64_t keep) {
    const auto data = mem::r64(self);
    const auto rep = data - 24;
    const bool shared = rep == emptyRep() || std::int32_t(mem::r32(rep + 16)) > 0;
    if (!shared && capacity(data) >= cap) return data;
    std::uint64_t newCap = std::max(cap, capacity(data) * 2);
    if (newCap < 16) newCap = 16;
    const auto fresh = newRep(newCap) + 24;
    std::memcpy(mem::base() + fresh, mem::base() + data, std::size_t(std::min(keep, length(data))));
    mem::w64(fresh - 24, std::min(keep, length(data)));
    dispose(data);
    mem::w64(self, fresh);
    return fresh;
}
void setLength(std::uint64_t data, std::uint64_t n) {
    if (data - 24 == emptyRep()) return;
    mem::w64(data - 24, n);
    mem::base()[data + n] = 0;
}
} // namespace
static hle::DataRegistrar d_emptyRep("__ZNSs4_Rep20_S_empty_rep_storageE", [] { return emptyRep(); });
HLE(_ZNSsC1EPKcRKSaIcE) { // string(const char *, const allocator &)
    const auto s = arg(1);
    mem::w64(arg(0), makeData(mem::base() + s, s ? std::strlen(reinterpret_cast<char *>(mem::base() + s)) : 0));
}
HLE(_ZNSsC1EPKcmRKSaIcE) { mem::w64(arg(0), makeData(mem::base() + arg(1), arg(2))); }
HLE(_ZNSsC1ERKSs) { // copy: a private copy (sharing is only an optimisation)
    const auto other = mem::r64(arg(1));
    mem::w64(arg(0), makeData(mem::base() + other, length(other)));
}
HLE(_ZNSsD1Ev) { dispose(mem::r64(arg(0))); }
HLE(_ZNSs4_Rep10_M_destroyERKSaIcE) { if (arg(0) != emptyRep()) mem::free(arg(0)); }
HLE(_ZNSs6assignEPKcm) {
    const auto self = arg(0), s = arg(1), n = arg(2);
    std::vector<std::uint8_t> copy(mem::base() + s, mem::base() + s + n);
    const auto data = reserveUnique(self, n, 0);
    std::memcpy(mem::base() + data, copy.data(), std::size_t(n));
    setLength(data, n);
    ret(self);
}
HLE(_ZNSs6assignERKSs) {
    const auto self = arg(0), other = mem::r64(arg(1));
    if (mem::r64(self) == other) return ret(self);
    const auto fresh = makeData(mem::base() + other, length(other));
    dispose(mem::r64(self));
    mem::w64(self, fresh);
    ret(self);
}
HLE(_ZNSs9push_backEc) {
    const auto self = arg(0);
    const auto n = length(mem::r64(self));
    const auto data = reserveUnique(self, n + 1, n);
    mem::base()[data + n] = std::uint8_t(arg(1));
    setLength(data, n + 1);
}
HLE(_ZNSs6appendEPKcm) {
    const auto self = arg(0), s = arg(1), add = arg(2);
    std::vector<std::uint8_t> copy(mem::base() + s, mem::base() + s + add);
    const auto n = length(mem::r64(self));
    const auto data = reserveUnique(self, n + add, n);
    std::memcpy(mem::base() + data + n, copy.data(), std::size_t(add));
    setLength(data, n + add);
    ret(self);
}
HLE(_ZNSs6appendERKSs) {
    const auto self = arg(0), other = mem::r64(arg(1));
    const auto add = length(other);
    std::vector<std::uint8_t> copy(mem::base() + other, mem::base() + other + add);
    const auto n = length(mem::r64(self));
    const auto data = reserveUnique(self, n + add, n);
    std::memcpy(mem::base() + data + n, copy.data(), std::size_t(add));
    setLength(data, n + add);
    ret(self);
}
// _M_mutate(pos, len1, len2): replace len1 characters at pos by len2 (uninitialised) characters, keeping the tail.
HLE(_ZNSs9_M_mutateEmmm) {
    const auto self = arg(0), pos = arg(1), len1 = arg(2), len2 = arg(3);
    const auto old = mem::r64(self);
    const auto oldLen = length(old);
    const auto tail = oldLen - pos - len1;
    const auto newLen = oldLen + len2 - len1;
    std::vector<std::uint8_t> before(mem::base() + old, mem::base() + old + oldLen);
    const auto data = reserveUnique(self, newLen, 0);
    std::memcpy(mem::base() + data, before.data(), std::size_t(pos));
    std::memcpy(mem::base() + data + pos + len2, before.data() + pos + len1, std::size_t(tail));
    setLength(data, newLen);
}
HLE(_ZNKSs7compareERKSs) {
    const auto a = mem::r64(arg(0)), b = mem::r64(arg(1));
    const auto la = length(a), lb = length(b);
    int r = std::memcmp(mem::base() + a, mem::base() + b, std::size_t(std::min(la, lb)));
    if (!r) r = la < lb ? -1 : la > lb ? 1 : 0;
    ret(std::uint64_t(std::int64_t(r)));
}
HLE(_ZNKSs7compareEPKc) {
    const auto a = mem::r64(arg(0));
    const std::string other = mem::cstring(arg(1));
    const std::string mine(reinterpret_cast<char *>(mem::base() + a), std::size_t(length(a)));
    ret(std::uint64_t(std::int64_t(mine.compare(other))));
}

// --- Red-black tree helpers (_Rb_tree_node_base: color +0 (0 red, 1 black), parent +8, left +16, right +24) ---
namespace {
using N = std::uint64_t;
N parent(N n) { return mem::r64(n + 8); }
N left(N n) { return mem::r64(n + 16); }
N right(N n) { return mem::r64(n + 24); }
int color(N n) { return int(mem::r32(n)); }
void setParent(N n, N v) { mem::w64(n + 8, v); }
void setLeft(N n, N v) { mem::w64(n + 16, v); }
void setRight(N n, N v) { mem::w64(n + 24, v); }
void setColor(N n, int c) { mem::w32(n, std::uint32_t(c)); }
constexpr int kRed = 0, kBlack = 1;
N increment(N x) {
    if (right(x)) {
        x = right(x);
        while (left(x)) x = left(x);
    } else {
        N y = parent(x);
        while (x == right(y)) { x = y; y = parent(y); }
        if (right(x) != y) x = y;
    }
    return x;
}
N decrement(N x) {
    if (color(x) == kRed && parent(parent(x)) == x) x = right(x); // header
    else if (left(x)) {
        N y = left(x);
        while (right(y)) y = right(y);
        x = y;
    } else {
        N y = parent(x);
        while (x == left(y)) { x = y; y = parent(y); }
        x = y;
    }
    return x;
}
// root is the header's parent field (header + 8).
void rotateLeft(N x, N rootSlot) {
    const N y = right(x);
    setRight(x, left(y));
    if (left(y)) setParent(left(y), x);
    setParent(y, parent(x));
    if (x == mem::r64(rootSlot)) mem::w64(rootSlot, y);
    else if (x == left(parent(x))) setLeft(parent(x), y);
    else setRight(parent(x), y);
    setLeft(y, x);
    setParent(x, y);
}
void rotateRight(N x, N rootSlot) {
    const N y = left(x);
    setLeft(x, right(y));
    if (right(y)) setParent(right(y), x);
    setParent(y, parent(x));
    if (x == mem::r64(rootSlot)) mem::w64(rootSlot, y);
    else if (x == right(parent(x))) setRight(parent(x), y);
    else setLeft(parent(x), y);
    setRight(y, x);
    setParent(x, y);
}
} // namespace
HLE(_ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base) { ret(increment(arg(0))); }
HLE(_ZSt18_Rb_tree_incrementPKSt18_Rb_tree_node_base) { ret(increment(arg(0))); }
HLE(_ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base) { ret(decrement(arg(0))); }
HLE(_ZSt18_Rb_tree_decrementPKSt18_Rb_tree_node_base) { ret(decrement(arg(0))); }
// _Rb_tree_insert_and_rebalance(bool insert_left, node x, node p, header &)
HLE(_ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_) {
    const bool insertLeft = (arg(0) & 0xFF) != 0;
    N x = arg(1);
    const N p = arg(2), header = arg(3);
    const N rootSlot = header + 8;
    setParent(x, p);
    setLeft(x, 0);
    setRight(x, 0);
    setColor(x, kRed);
    if (insertLeft) {
        setLeft(p, x);
        if (p == header) { setParent(header, x); setRight(header, x); }
        else if (p == left(header)) setLeft(header, x);
    } else {
        setRight(p, x);
        if (p == right(header)) setRight(header, x);
    }
    while (x != mem::r64(rootSlot) && color(parent(x)) == kRed) {
        const N xpp = parent(parent(x));
        if (parent(x) == left(xpp)) {
            const N y = right(xpp);
            if (y && color(y) == kRed) {
                setColor(parent(x), kBlack);
                setColor(y, kBlack);
                setColor(xpp, kRed);
                x = xpp;
            } else {
                if (x == right(parent(x))) { x = parent(x); rotateLeft(x, rootSlot); }
                setColor(parent(x), kBlack);
                setColor(xpp, kRed);
                rotateRight(xpp, rootSlot);
            }
        } else {
            const N y = left(xpp);
            if (y && color(y) == kRed) {
                setColor(parent(x), kBlack);
                setColor(y, kBlack);
                setColor(xpp, kRed);
                x = xpp;
            } else {
                if (x == left(parent(x))) { x = parent(x); rotateRight(x, rootSlot); }
                setColor(parent(x), kBlack);
                setColor(xpp, kRed);
                rotateLeft(xpp, rootSlot);
            }
        }
    }
    setColor(mem::r64(rootSlot), kBlack);
}
// _Rb_tree_rebalance_for_erase(node z, header &) -> the node to free (z).
HLE(_ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_) {
    const N z = arg(0), header = arg(1);
    const N rootSlot = header + 8, leftmostSlot = header + 16, rightmostSlot = header + 24;
    N y = z, x = 0, xParent = 0;
    if (!left(y)) x = right(y);
    else if (!right(y)) x = left(y);
    else {
        y = right(y);
        while (left(y)) y = left(y);
        x = right(y);
    }
    if (y != z) {
        setParent(left(z), y);
        setLeft(y, left(z));
        if (y != right(z)) {
            xParent = parent(y);
            if (x) setParent(x, parent(y));
            setLeft(parent(y), x);
            setRight(y, right(z));
            setParent(right(z), y);
        } else xParent = y;
        if (mem::r64(rootSlot) == z) mem::w64(rootSlot, y);
        else if (left(parent(z)) == z) setLeft(parent(z), y);
        else setRight(parent(z), y);
        setParent(y, parent(z));
        const int c = color(y);
        setColor(y, color(z));
        setColor(z, c);
        y = z;
    } else {
        xParent = parent(y);
        if (x) setParent(x, parent(y));
        if (mem::r64(rootSlot) == z) mem::w64(rootSlot, x);
        else if (left(parent(z)) == z) setLeft(parent(z), x);
        else setRight(parent(z), x);
        if (mem::r64(leftmostSlot) == z) {
            if (!right(z)) mem::w64(leftmostSlot, parent(z));
            else { N m = x; while (left(m)) m = left(m); mem::w64(leftmostSlot, m); }
        }
        if (mem::r64(rightmostSlot) == z) {
            if (!left(z)) mem::w64(rightmostSlot, parent(z));
            else { N m = x; while (right(m)) m = right(m); mem::w64(rightmostSlot, m); }
        }
    }
    if (color(y) != kRed) {
        while (x != mem::r64(rootSlot) && (!x || color(x) == kBlack)) {
            if (x == left(xParent)) {
                N w = right(xParent);
                if (color(w) == kRed) {
                    setColor(w, kBlack);
                    setColor(xParent, kRed);
                    rotateLeft(xParent, rootSlot);
                    w = right(xParent);
                }
                if ((!left(w) || color(left(w)) == kBlack) && (!right(w) || color(right(w)) == kBlack)) {
                    setColor(w, kRed);
                    x = xParent;
                    xParent = parent(xParent);
                } else {
                    if (!right(w) || color(right(w)) == kBlack) {
                        setColor(left(w), kBlack);
                        setColor(w, kRed);
                        rotateRight(w, rootSlot);
                        w = right(xParent);
                    }
                    setColor(w, color(xParent));
                    setColor(xParent, kBlack);
                    if (right(w)) setColor(right(w), kBlack);
                    rotateLeft(xParent, rootSlot);
                    break;
                }
            } else {
                N w = left(xParent);
                if (color(w) == kRed) {
                    setColor(w, kBlack);
                    setColor(xParent, kRed);
                    rotateRight(xParent, rootSlot);
                    w = left(xParent);
                }
                if ((!right(w) || color(right(w)) == kBlack) && (!left(w) || color(left(w)) == kBlack)) {
                    setColor(w, kRed);
                    x = xParent;
                    xParent = parent(xParent);
                } else {
                    if (!left(w) || color(left(w)) == kBlack) {
                        setColor(right(w), kBlack);
                        setColor(w, kRed);
                        rotateLeft(w, rootSlot);
                        w = left(xParent);
                    }
                    setColor(w, color(xParent));
                    setColor(xParent, kBlack);
                    if (left(w)) setColor(left(w), kBlack);
                    rotateRight(xParent, rootSlot);
                    break;
                }
            }
        }
        if (x) setColor(x, kBlack);
    }
    ret(y);
}

// std::ofstream, as InitState::contentUpdateDone uses it to save the new fingerprint.json: the stream object (guest
// memory the caller owns) maps to a host file. libstdc++ _Ios_Openmode: app 1, ate 2, binary 4, in 8, out 16, trunc 32.
namespace {
std::mutex g_ofstreamLock;
std::map<std::uint64_t, std::FILE *> g_ofstreams;
void closeOfstream(std::uint64_t stream) {
    std::lock_guard lock(g_ofstreamLock);
    auto it = g_ofstreams.find(stream);
    if (it == g_ofstreams.end()) return;
    if (it->second) std::fclose(it->second);
    g_ofstreams.erase(it);
}
} // namespace
// basic_ofstream(const char *path, ios_base::openmode mode)
HLE(_ZNSt14basic_ofstreamIcSt11char_traitsIcEEC1EPKcSt13_Ios_Openmode) {
    const auto stream = arg(0);
    const std::string guestPath = reinterpret_cast<const char *>(mem::base() + arg(1));
    const auto mode = arg(2);
    closeOfstream(stream);
    std::FILE *f = platform::openFile(files::hostPath(guestPath), (mode & 1) ? "ab" : "wb");
    hlog("file", "ofstream %s mode=%llu%s", guestPath.c_str(), (unsigned long long)mode, f ? "" : " (failed)");
    std::lock_guard lock(g_ofstreamLock);
    g_ofstreams[stream] = f;
}
// basic_ostream::write(const char *data, streamsize count) -> *this
HLE(_ZNSo5writeEPKcl) {
    {
        std::lock_guard lock(g_ofstreamLock);
        auto it = g_ofstreams.find(arg(0));
        if (it != g_ofstreams.end() && it->second) std::fwrite(mem::base() + arg(1), 1, std::size_t(arg(2)), it->second);
    }
    ret(arg(0));
}
HLE(_ZNSt14basic_ofstreamIcSt11char_traitsIcEE5closeEv) { closeOfstream(arg(0)); }
HLE(_ZNSt14basic_ofstreamIcSt11char_traitsIcEED1Ev) { closeOfstream(arg(0)); }
