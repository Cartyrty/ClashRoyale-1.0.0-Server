#pragma once
// HLE function and data registry. Handlers read their arguments from the guest registers (Apple ARM64 ABI: x0-x7 /
// d0-d7, variadic arguments on the stack in 8-byte slots) and write results to x0 / d0.
#include "Cpu.h"
#include <cstdint>
#include <functional>
#include <string>

namespace hle {
using Handler = void (*)();
// Mach-O symbol names, with the leading underscore (e.g. "_malloc").
void registerFunction(const std::string &name, Handler handler);
// A data import: the factory returns the guest address of the variable, created on first use.
void registerData(const std::string &name, std::function<std::uint64_t()> factory);
// Data imports by name prefix (e.g. "_OBJC_CLASS_$_"): the factory receives the full name.
void registerDataPrefix(const std::string &prefix, std::function<std::uint64_t(const std::string &)> factory);
// The address an import binds to (a trap stub for functions, the variable for data).
std::uint64_t resolveImport(const std::string &name);
// A trap stub calling a host handler; used for host-implemented IMPs and callbacks handed to the guest.
std::uint64_t makeStub(const std::string &name, Handler handler);
void dispatch(std::uint32_t swi);
// Replaces a function in the app image with a host handler (its first two instructions become "SVC #n; RET").
bool patchFunction(const std::string &symbol, Handler handler);
// Runs a host handler on entry to an app function, then the original function (same registers): the first two
// instructions move to a trampoline. Refused when they are PC-relative.
bool hookFunction(const std::string &symbol, Handler before);
// Patches registered at static-init time, applied after the image is loaded.
void applyPatches();
// Work to run once the image is loaded (e.g. hooks enabled by environment variables).
void afterLoad(std::function<void()> fn);
struct PatchRegistrar {
    PatchRegistrar(const char *symbol, Handler handler);
};
bool looksLikeData(const std::string &name);
void createSpecialStubs();
const std::string &currentFunction();

struct Registrar {
    Registrar(const char *name, Handler handler) { registerFunction(name, handler); }
};
struct DataRegistrar {
    DataRegistrar(const char *name, std::function<std::uint64_t()> factory) { registerData(name, std::move(factory)); }
};

inline std::uint64_t arg(int i) { return cpu::x(i); }
inline void ret(std::uint64_t value) { cpu::setX(0, value); }
inline void retDouble(double value) { cpu::setD(0, value); }
inline void retFloat(float value) { cpu::setS(0, value); }
// The i-th variadic argument when the named arguments occupy registers: Apple passes all variadic arguments on the stack.
inline std::uint64_t varg(int i);
} // namespace hle

#include "Memory.h"
inline std::uint64_t hle::varg(int i) { return mem::r64(cpu::sp() + 8 * std::uint64_t(i)); }

#define HLE_CAT2(a, b) a##b
#define HLE_CAT(a, b) HLE_CAT2(a, b)
// HLE(malloc) { ... } defines the handler for the C symbol _malloc.
#define HLE(name)                                                                                                      \
    static void HLE_CAT(hle_fn_, name)();                                                                              \
    static ::hle::Registrar HLE_CAT(hle_reg_, name)("_" #name, HLE_CAT(hle_fn_, name));                               \
    static void HLE_CAT(hle_fn_, name)()
// HLE_NAMED("_select$DARWIN_EXTSN", select_darwin) { ... } for symbol names that are not identifiers.
#define HLE_NAMED(symbol, id)                                                                                          \
    static void HLE_CAT(hle_fn_, id)();                                                                                \
    static ::hle::Registrar HLE_CAT(hle_reg_, id)(symbol, HLE_CAT(hle_fn_, id));                                      \
    static void HLE_CAT(hle_fn_, id)()
