#include "Cpu.h"
#include "Hle.h"
#include "Loader.h"
#include "Log.h"
#include "Memory.h"
#include <dynarmic/interface/A64/a64.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif
#include <dynarmic/interface/A64/config.h>
#include <dynarmic/interface/exclusive_monitor.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <vector>


// 128-bit compare-and-swap for exclusive pair stores (MSVC intrinsic, or cmpxchg16b via -mcx16 on GCC/Clang).
static bool cas128(void *p, std::uint64_t lo, std::uint64_t hi, std::uint64_t expectLo, std::uint64_t expectHi) {
#ifdef _MSC_VER
    long long expected[2] = {(long long)expectLo, (long long)expectHi};
    return _InterlockedCompareExchange128(static_cast<volatile long long *>(p), (long long)hi, (long long)lo, expected) != 0;
#else
    // __sync (not __atomic): GCC emits cmpxchg16b inline for it with -mcx16, but calls libatomic for 16-byte __atomic.
    const unsigned __int128 expected = ((unsigned __int128)expectHi << 64) | expectLo;
    const unsigned __int128 desired = ((unsigned __int128)hi << 64) | lo;
    return __sync_bool_compare_and_swap(static_cast<unsigned __int128 *>(p), expected, desired);
#endif
}

namespace cpu {
namespace {
Dynarmic::ExclusiveMonitor *g_monitor = nullptr;
std::atomic<std::size_t> g_nextProcessor{0};
constexpr std::size_t kMaxThreads = 64;
std::uint64_t g_returnStub = 0, g_threadExitStub = 0;
std::mutex g_threadsLock;
std::vector<Thread *> g_threads;
} // namespace

struct Callbacks final : Dynarmic::A64::UserCallbacks {
    Thread *thread = nullptr;
    template <class T> T load(std::uint64_t a);
    template <class T> void store(std::uint64_t a, T v);
    std::uint8_t MemoryRead8(std::uint64_t a) override { return load<std::uint8_t>(a); }
    std::uint16_t MemoryRead16(std::uint64_t a) override { return load<std::uint16_t>(a); }
    std::uint32_t MemoryRead32(std::uint64_t a) override { return load<std::uint32_t>(a); }
    std::uint64_t MemoryRead64(std::uint64_t a) override { return load<std::uint64_t>(a); }
    Dynarmic::A64::Vector MemoryRead128(std::uint64_t a) override { return {load<std::uint64_t>(a), load<std::uint64_t>(a + 8)}; }
    void MemoryWrite8(std::uint64_t a, std::uint8_t v) override { store(a, v); }
    void MemoryWrite16(std::uint64_t a, std::uint16_t v) override { store(a, v); }
    void MemoryWrite32(std::uint64_t a, std::uint32_t v) override { store(a, v); }
    void MemoryWrite64(std::uint64_t a, std::uint64_t v) override { store(a, v); }
    void MemoryWrite128(std::uint64_t a, Dynarmic::A64::Vector v) override { store(a, v[0]); store(a + 8, v[1]); }
    bool MemoryWriteExclusive8(std::uint64_t a, std::uint8_t v, std::uint8_t e) override { return cas(a, v, e); }
    bool MemoryWriteExclusive16(std::uint64_t a, std::uint16_t v, std::uint16_t e) override { return cas(a, v, e); }
    bool MemoryWriteExclusive32(std::uint64_t a, std::uint32_t v, std::uint32_t e) override { return cas(a, v, e); }
    bool MemoryWriteExclusive64(std::uint64_t a, std::uint64_t v, std::uint64_t e) override { return cas(a, v, e); }
    bool MemoryWriteExclusive128(std::uint64_t a, Dynarmic::A64::Vector v, Dynarmic::A64::Vector e) override {
        if (!mem::valid(a, 16)) return false;
        return cas128(mem::base() + a, v[0], v[1], e[0], e[1]); // [0] = low 64 bits
    }
    template <class T> bool cas(std::uint64_t a, T v, T e) {
        if (!mem::valid(a, sizeof(T))) return false;
        auto *p = reinterpret_cast<std::atomic<T> *>(mem::base() + a);
        return p->compare_exchange_strong(e, v);
    }
    void InterpreterFallback(std::uint64_t pc, std::size_t n) override;
    void CallSVC(std::uint32_t swi) override;
    void ExceptionRaised(std::uint64_t pc, Dynarmic::A64::Exception e) override;
    void AddTicks(std::uint64_t) override {}
    std::uint64_t GetTicksRemaining() override { return ~0ull >> 2; }
    std::uint64_t GetCNTPCT() override {
        // 24 MHz like the device's timebase.
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count() * 24 / 1000);
    }
};

struct Thread {
    Callbacks callbacks;
    std::unique_ptr<Dynarmic::A64::Jit> jit;
    std::uint64_t tpidrro = 0, tpidr = 0;
    std::uint32_t pendingSvc = 0;
    bool svcPending = false, faulted = false;
    std::uint64_t faultAddress = 0;
    double lastFloat = 0;
    int depth = 0;
    std::uint64_t stackTop = 0;
};

namespace {
thread_local Thread *t_current = nullptr;
}

template <class T> T Callbacks::load(std::uint64_t a) {
    if (!mem::valid(a, sizeof(T))) {
        if (!thread->faulted) { thread->faulted = true; thread->faultAddress = a; thread->jit->HaltExecution(Dynarmic::HaltReason::UserDefined2); }
        return T{};
    }
    return mem::read<T>(a);
}
template <class T> void Callbacks::store(std::uint64_t a, T v) {
    if (!mem::valid(a, sizeof(T))) {
        if (!thread->faulted) { thread->faulted = true; thread->faultAddress = a; thread->jit->HaltExecution(Dynarmic::HaltReason::UserDefined2); }
        return;
    }
    mem::write<T>(a, v);
}
void Callbacks::InterpreterFallback(std::uint64_t pc, std::size_t n) {
    fatal("unimplemented ARM64 instruction at %s (%zu instructions, word %08x)", loader::describe(pc).c_str(), n, mem::r32(pc));
}
void Callbacks::CallSVC(std::uint32_t swi) {
    thread->pendingSvc = swi;
    thread->svcPending = true;
    thread->jit->HaltExecution(Dynarmic::HaltReason::UserDefined1);
}
void Callbacks::ExceptionRaised(std::uint64_t pc, Dynarmic::A64::Exception e) {
    using E = Dynarmic::A64::Exception;
    if (e == E::Yield || e == E::WaitForEvent || e == E::SendEvent || e == E::SendEventLocal || e == E::WaitForInterrupt) {
        thread->jit->SetPC(pc + 4);
        return;
    }
    thread->jit->SetPC(pc);
    backtrace(e == E::Breakpoint ? "BRK (trap / assertion)" : "undefined instruction");
    fatal("guest exception %d at %s", int(e), loader::describe(pc).c_str());
}

void initProcess() { g_monitor = new Dynarmic::ExclusiveMonitor(kMaxThreads); }
void setReturnStub(std::uint64_t address) { g_returnStub = address; }
void setThreadExitStub(std::uint64_t address) { g_threadExitStub = address; }

Thread &attachThread(std::uint64_t stackTop, std::size_t codeCacheBytes) {
    auto *t = new Thread;
    t->callbacks.thread = t;
    t->stackTop = stackTop;
    // The thread pointer: libSystem keeps the pthread struct there; HLE code uses it as the TLS block.
    t->tpidrro = mem::calloc(0x1000);
    Dynarmic::A64::UserConfig config;
    config.callbacks = &t->callbacks;
    config.processor_id = g_nextProcessor++ % kMaxThreads;
    config.global_monitor = g_monitor;
    config.fastmem_pointer = reinterpret_cast<std::uintptr_t>(mem::base());
    config.fastmem_address_space_bits = mem::kAddressBits;
    config.silently_mirror_fastmem = false;
    config.tpidrro_el0 = &t->tpidrro;
    config.tpidr_el0 = &t->tpidr;
    config.enable_cycle_counting = false;
    config.code_cache_size = codeCacheBytes;
    config.cntfrq_el0 = 24000000;
    config.dczid_el0 = 4;
    config.ctr_el0 = 0x8444c004;
    t->jit = std::make_unique<Dynarmic::A64::Jit>(config);
    t->jit->SetSP(stackTop);
    t->jit->SetFpcr(0);
    t_current = t;
    std::lock_guard lock(g_threadsLock);
    g_threads.push_back(t);
    return *t;
}
Thread &current() { return *t_current; }
std::uint64_t threadPointer() { return t_current ? t_current->tpidrro : 0; }
bool hasCurrent() { return t_current != nullptr; }

std::uint64_t x(int i) { return i == 31 ? 0 : t_current->jit->GetRegister(std::size_t(i)); }
void setX(int i, std::uint64_t v) { t_current->jit->SetRegister(std::size_t(i), v); }
double d(int i) {
    double value;
    const auto v = t_current->jit->GetVector(std::size_t(i));
    std::memcpy(&value, &v[0], 8);
    return value;
}
float s(int i) {
    float value;
    const auto v = t_current->jit->GetVector(std::size_t(i));
    std::memcpy(&value, &v[0], 4);
    return value;
}
void setD(int i, double value) {
    Dynarmic::A64::Vector v{0, 0};
    std::memcpy(&v[0], &value, 8);
    t_current->jit->SetVector(std::size_t(i), v);
}
void setS(int i, float value) {
    Dynarmic::A64::Vector v{0, 0};
    std::memcpy(&v[0], &value, 4);
    t_current->jit->SetVector(std::size_t(i), v);
}
std::uint64_t sp() { return t_current->jit->GetSP(); }
void setSp(std::uint64_t value) { t_current->jit->SetSP(value); }
std::uint64_t pc() { return t_current->jit->GetPC(); }
void setPc(std::uint64_t value) { t_current->jit->SetPC(value); }
std::uint64_t lr() { return x(30); }
std::uint32_t current_svc() { return t_current->pendingSvc; }
double lastFloatResult() { return t_current->lastFloat; }
void invalidateCode(std::uint64_t address, std::size_t length) {
    std::lock_guard lock(g_threadsLock);
    for (auto *t : g_threads) t->jit->InvalidateCacheRange(address, length);
}

// Runs until the stub at g_returnStub executes (the function returned).
static void runLoop() {
    Thread &t = *t_current;
    for (;;) {
        t.jit->Run();
        if (t.faulted) {
            t.faulted = false;
            backtrace("memory fault");
            fatal("invalid memory access at %llx from %s", (unsigned long long)t.faultAddress, loader::describe(t.jit->GetPC()).c_str());
        }
        if (!t.svcPending) continue;
        t.svcPending = false;
        const auto swi = t.pendingSvc;
        if (swi == kReturnSvc) return;
        hle::dispatch(swi);
    }
}

std::uint64_t call(std::uint64_t function, std::initializer_list<std::uint64_t> args, std::initializer_list<double> floatArgs) {
    Thread &t = *t_current;
    auto *jit = t.jit.get();
    // Save the interrupted guest context (we are inside an HLE handler, or at top level).
    const auto regs = jit->GetRegisters();
    const auto vectors = jit->GetVectors();
    const auto savedSp = jit->GetSP(), savedPc = jit->GetPC();
    const auto savedFpcr = jit->GetFpcr(), savedPstate = jit->GetPstate();
    int i = 0;
    for (auto a : args) jit->SetRegister(std::size_t(i++), a);
    i = 0;
    for (auto f : floatArgs) setD(i++, f);
    // Leave the 128-byte red zone below the interrupted frame alone.
    jit->SetSP(((savedSp - 128) & ~0xFull));
    jit->SetRegister(30, g_returnStub);
    jit->SetPC(function);
    ++t.depth;
    runLoop();
    --t.depth;
    const auto result = jit->GetRegister(0);
    t.lastFloat = d(0);
    jit->SetRegisters(regs);
    jit->SetVectors(vectors);
    jit->SetSP(savedSp);
    jit->SetPC(savedPc);
    jit->SetFpcr(savedFpcr);
    jit->SetPstate(savedPstate);
    return result;
}

std::uint64_t callFloats(std::uint64_t function, std::initializer_list<std::uint64_t> args, std::initializer_list<float> floatArgs) {
    // Float arguments: d registers are saved/restored by call(); set the s registers through the double slots.
    std::vector<double> asDoubles;
    for (float f : floatArgs) {
        double d = 0;
        std::uint64_t bits = 0;
        std::memcpy(&bits, &f, 4);
        std::memcpy(&d, &bits, 8);
        asDoubles.push_back(d);
    }
    switch (asDoubles.size()) {
    case 1: return call(function, args, {asDoubles[0]});
    case 2: return call(function, args, {asDoubles[0], asDoubles[1]});
    case 3: return call(function, args, {asDoubles[0], asDoubles[1], asDoubles[2]});
    default: return call(function, args, {asDoubles[0], asDoubles[1], asDoubles[2], asDoubles[3]});
    }
}

std::uint64_t runThread(std::uint64_t function, std::initializer_list<std::uint64_t> args) {
    Thread &t = *t_current;
    t.jit->SetSP(t.stackTop);
    t.jit->SetRegister(29, 0);
    return call(function, args);
}

void backtrace(const char *why) {
    if (!t_current) return;
    auto *jit = t_current->jit.get();
    std::fprintf(stderr, "[crash] %s\n[crash]   pc %s\n[crash]   lr %s\n", why, loader::describe(jit->GetPC()).c_str(),
                 loader::describe(jit->GetRegister(30)).c_str());
    for (int r = 0; r < 31; r += 4) {
        std::fprintf(stderr, "[crash]  ");
        for (int k = r; k < r + 4 && k < 31; ++k) std::fprintf(stderr, " x%-2d=%016llx", k, (unsigned long long)jit->GetRegister(std::size_t(k)));
        std::fprintf(stderr, "\n");
    }
    std::fprintf(stderr, "[crash]   sp=%016llx\n", (unsigned long long)jit->GetSP());
    // Frame-pointer chain: [fp] = previous fp, [fp+8] = return address.
    std::uint64_t fp = jit->GetRegister(29);
    for (int frame = 0; frame < 32 && fp && mem::valid(fp, 16); ++frame) {
        const auto ret = mem::r64(fp + 8);
        std::fprintf(stderr, "[crash]   #%d %s\n", frame, loader::describe(ret).c_str());
        const auto next = mem::r64(fp);
        if (next <= fp) break;
        fp = next;
    }
    std::fflush(stderr);
}
} // namespace cpu
