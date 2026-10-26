#include <exception>
#include "Hle.h"
#include "Loader.h"
#include "Log.h"
#include "Memory.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace hle {
namespace {
struct Stub {
    std::string name;
    Handler handler = nullptr;
    std::uint64_t calls = 0;
};
std::unordered_map<std::string, Handler> &functions() { static std::unordered_map<std::string, Handler> m; return m; }
std::unordered_map<std::string, std::function<std::uint64_t()>> &dataFactories() {
    static std::unordered_map<std::string, std::function<std::uint64_t()>> m;
    return m;
}
std::vector<std::pair<std::string, std::function<std::uint64_t(const std::string &)>>> &prefixFactories() {
    static std::vector<std::pair<std::string, std::function<std::uint64_t(const std::string &)>>> v;
    return v;
}
std::vector<Stub> g_stubs;
std::unordered_map<std::string, std::uint64_t> g_resolved;
std::unordered_map<std::uint64_t, std::uint32_t> g_stubAt;
std::recursive_mutex g_lock;
thread_local const std::string *t_function = nullptr;

constexpr std::uint32_t svc(std::uint32_t n) { return 0xD4000001u | (n << 5); }
constexpr std::uint32_t kRet = 0xD65F03C0u;

std::uint64_t emitStub(std::uint32_t number) {
    const auto address = mem::hleAlloc(8, 8);
    mem::w32(address, svc(number));
    mem::w32(address + 4, kRet);
    return address;
}
} // namespace
// Names that are variables rather than functions when the registry has no entry for them.
bool looksLikeData(const std::string &name) {
    static const char *prefixes[] = {"_OBJC_CLASS_$_", "_OBJC_METACLASS_$_", "_OBJC_EHTYPE_$_", "__NSConcrete", "_kCF", "_kCT",
                                     "_kEAGL", "_kSec", "_kCG", "_kAB", "_kSC", "_kUT", "___stack_chk_guard", "___stderrp",
                                     "___stdoutp", "___stdinp", "__DefaultRuneLocale", "_mach_task_self_", "_vm_page_size",
                                     "_vm_page_mask", "_NDR_record", "__dispatch_main_q", "___CFConstantStringClassReference",
                                     "_CGRectZero", "_CGSizeZero", "_CGPointZero", "_CGAffineTransformIdentity",
                                     "_UIEdgeInsetsZero", "_NSFoundationVersionNumber", "_UIBackgroundTaskInvalid",
                                     "_UIWindowLevel", "_GKPlayerAuthenticationDidChangeNotificationName", "__ZNSs4_Rep20_S_empty_rep_storageE"};
    for (const char *p : prefixes) if (name.rfind(p, 0) == 0) return true;
    // NSString constants: _NSFooKey, _UIFooNotification, _NSFooDomain, _NSFooException, _NSFooMode, _NSFooAttributeName...
    static const char *suffixes[] = {"Key", "Notification", "Domain", "Exception", "Mode", "Name", "Modes", "Type", "Size",
                                     "Date", "Permissions", "Regular", "SoundName", "Trust", "Calendar", "Code", "Symbol",
                                     "Original", "ModeKey"};
    const bool framework = name.rfind("_NS", 0) == 0 || name.rfind("_UI", 0) == 0 || name.rfind("_AV", 0) == 0 ||
                           name.rfind("_MP", 0) == 0 || name.rfind("_GK", 0) == 0 || name.rfind("_SK", 0) == 0 ||
                           name.rfind("_AD", 0) == 0 || name.rfind("_CT", 0) == 0 || name.rfind("_SL", 0) == 0 ||
                           name.rfind("_AC", 0) == 0 || name.rfind("_FB", 0) == 0;
    if (framework)
        for (const char *s : suffixes) {
            const std::string suffix = s;
            if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0) return true;
        }
    return false;
}

void registerFunction(const std::string &name, Handler handler) { functions()[name] = handler; }
void registerData(const std::string &name, std::function<std::uint64_t()> factory) { dataFactories()[name] = std::move(factory); }
void registerDataPrefix(const std::string &prefix, std::function<std::uint64_t(const std::string &)> factory) {
    prefixFactories().push_back({prefix, std::move(factory)});
}
const std::string &currentFunction() {
    static const std::string none;
    return t_function ? *t_function : none;
}

std::uint64_t makeStub(const std::string &name, Handler handler) {
    std::lock_guard lock(g_lock);
    const auto number = std::uint32_t(g_stubs.size());
    if (number >= 0xFF00) fatal("too many HLE stubs");
    g_stubs.push_back({name, handler, 0});
    const auto address = emitStub(number);
    g_stubAt[address] = number;
    g_stubAt[address + 4] = number;
    return address;
}

std::uint64_t resolveImport(const std::string &name) {
    std::lock_guard lock(g_lock);
    if (auto found = g_resolved.find(name); found != g_resolved.end()) return found->second;
    std::uint64_t address = 0;
    if (auto data = dataFactories().find(name); data != dataFactories().end()) {
        address = data->second();
    } else if (auto fn = functions().find(name); fn != functions().end()) {
        address = makeStub(name, fn->second);
    } else {
        // Prefix factories may decline (return 0), e.g. the NSString constant factory for function names.
        for (const auto &[prefix, factory] : prefixFactories())
            if (name.rfind(prefix, 0) == 0 && (address = factory(name))) break;
        if (!address) {
            if (looksLikeData(name)) {
                // An unknown variable: zero-filled storage so reads do not fault; logged so it can be implemented.
                address = mem::hleAlloc(256, 16);
                hlog("data", "unimplemented variable %s -> %llx", name.c_str(), (unsigned long long)address);
            } else {
                address = makeStub(name, nullptr);
            }
        }
    }
    g_resolved[name] = address;
    return address;
}

void createSpecialStubs() {
    const auto ret = mem::hleAlloc(8, 8);
    mem::w32(ret, svc(cpu::kReturnSvc));
    mem::w32(ret + 4, kRet);
    cpu::setReturnStub(ret);
    const auto exit = mem::hleAlloc(8, 8);
    mem::w32(exit, svc(cpu::kThreadExitSvc));
    mem::w32(exit + 4, kRet);
    cpu::setThreadExitStub(exit);
}

bool patchFunction(const std::string &symbol, Handler handler) {
    const auto address = loader::symbol(symbol);
    if (!address) { warn("patch target %s not found", symbol.c_str()); return false; }
    std::uint32_t number;
    {
        std::lock_guard lock(g_lock);
        number = std::uint32_t(g_stubs.size());
        g_stubs.push_back({symbol + " [patched]", handler, 0});
        g_stubAt[address] = number;
        g_stubAt[address + 4] = number;
    }
    mem::w32(address, svc(number));
    mem::w32(address + 4, kRet);
    cpu::invalidateCode(address, 8);
    hlog("patch", "%s replaced by a host implementation", symbol.c_str());
    return true;
}
namespace {
struct Hook { Handler before; std::uint64_t trampoline; };
std::unordered_map<std::uint32_t, Hook> g_hooks;
bool pcRelative(std::uint32_t insn) {
    return (insn & 0x1F000000) == 0x10000000 ||  // ADR / ADRP
           (insn & 0x7C000000) == 0x14000000 ||  // B / BL
           (insn & 0x3B000000) == 0x18000000 ||  // LDR (literal)
           (insn & 0x7E000000) == 0x34000000 ||  // CBZ / CBNZ
           (insn & 0x7E000000) == 0x36000000 ||  // TBZ / TBNZ
           (insn & 0xFF000010) == 0x54000000;    // B.cond
}
} // namespace
bool hookFunction(const std::string &symbol, Handler before) {
    const auto address = loader::symbol(symbol);
    if (!address) { warn("hook target %s not found", symbol.c_str()); return false; }
    const auto i0 = mem::r32(address), i1 = mem::r32(address + 4);
    if (pcRelative(i0) || pcRelative(i1)) { warn("hook target %s starts with PC-relative code", symbol.c_str()); return false; }
    // Trampoline: the two original instructions, then LDR x16, #8; BR x16; .quad address + 8.
    const auto trampoline = mem::hleAlloc(24, 8);
    mem::w32(trampoline, i0);
    mem::w32(trampoline + 4, i1);
    mem::w32(trampoline + 8, 0x58000050);  // ldr x16, #8
    mem::w32(trampoline + 12, 0xD61F0200); // br x16
    mem::w64(trampoline + 16, address + 8);
    std::uint32_t number;
    {
        std::lock_guard lock(g_lock);
        number = std::uint32_t(g_stubs.size());
        g_stubs.push_back({symbol + " [hooked]", [] {
            const auto it = g_hooks.find(cpu::current_svc());
            if (it == g_hooks.end()) return;
            it->second.before();
            cpu::setPc(it->second.trampoline);
        }, 0});
        g_hooks[number] = {before, trampoline};
    }
    mem::w32(address, svc(number));
    mem::w32(address + 4, 0xD503201F); // nop (never reached)
    cpu::invalidateCode(address, 8);
    hlog("patch", "%s hooked", symbol.c_str());
    return true;
}
namespace {
std::vector<std::pair<std::string, Handler>> &patches() { static std::vector<std::pair<std::string, Handler>> v; return v; }
}
PatchRegistrar::PatchRegistrar(const char *symbol, Handler handler) { patches().push_back({symbol, handler}); }
namespace {
std::vector<std::function<void()>> &deferred() { static std::vector<std::function<void()>> v; return v; }
}
void afterLoad(std::function<void()> fn) { deferred().push_back(std::move(fn)); }
void applyPatches() {
    for (auto &[symbol, handler] : patches()) patchFunction(symbol, handler);
    for (auto &fn : deferred()) fn();
}

const char *stubName(std::uint64_t address) {
    std::lock_guard lock(g_lock);
    auto found = g_stubAt.find(address);
    return found == g_stubAt.end() ? nullptr : g_stubs[found->second].name.c_str();
}

void dispatch(std::uint32_t swi) {
    Stub *stub;
    {
        std::lock_guard lock(g_lock);
        if (swi >= g_stubs.size()) {
            cpu::backtrace("unknown SVC");
            fatal("unknown SVC #%u at %s", swi, loader::describe(cpu::pc()).c_str());
        }
        stub = &g_stubs[swi];
    }
    ++stub->calls;
    const std::string *previous = t_function;
    t_function = &stub->name;
    if (logEnabled("bridge")) hlog("bridge", "%s", stub->name.c_str());
    if (stub->handler) {
        try {
            stub->handler();
        } catch (const std::exception &e) {
            cpu::backtrace("host exception");
            fatal("host exception in %s (called from %s): %s", stub->name.c_str(), loader::describe(cpu::lr()).c_str(), e.what());
        }
    } else {
        if (stub->calls <= 3) hlog("trap", "unimplemented %s called from %s", stub->name.c_str(), loader::describe(cpu::lr()).c_str());
        cpu::setX(0, 0);
        cpu::setD(0, 0);
    }
    t_function = previous;
}
} // namespace hle
