#pragma once
// One dynarmic A64 JIT per guest thread. Imported functions are stubs "SVC #n; RET": the SVC callback records n and
// halts the JIT, the run loop dispatches the HLE handler outside of JIT execution (so handlers may call back into
// guest code by re-entering the same JIT), and execution resumes at the RET. Host -> guest calls set LR to a stub
// holding SVC #kReturnSvc, which ends that nested run.
#include <array>
#include <cstdint>
#include <initializer_list>
#include <memory>

namespace Dynarmic::A64 { class Jit; }

namespace cpu {
constexpr std::uint32_t kReturnSvc = 0xFFFF;
constexpr std::uint32_t kThreadExitSvc = 0xFFFE;

struct Thread;
// Sets up the JIT for the calling host thread (each guest thread runs on its own host thread).
Thread &attachThread(std::uint64_t stackTop, std::size_t codeCacheBytes = 128 * 1024 * 1024);
// The guest thread pointer (pthread_self / TPIDRRO_EL0) of the calling thread.
std::uint64_t threadPointer();
Thread &current();
bool hasCurrent();
void initProcess();

// Register access for the current thread (valid inside HLE handlers).
std::uint64_t x(int index);
void setX(int index, std::uint64_t value);
double d(int index);
float s(int index);
void setD(int index, double value);
void setS(int index, float value);
std::uint64_t sp();
void setSp(std::uint64_t value);
std::uint64_t pc();
void setPc(std::uint64_t value); // e.g. a tail call to a guest function (objc_msgSend)
std::uint64_t lr();
// The SVC number being dispatched on this thread.
std::uint32_t current_svc();

// Calls a guest function with integer arguments x0..x7 (and optional float arguments d0..d7); returns x0.
std::uint64_t call(std::uint64_t function, std::initializer_list<std::uint64_t> args = {},
                   std::initializer_list<double> floatArgs = {});
// Like call(), with single-precision float arguments in s0..s7.
std::uint64_t callFloats(std::uint64_t function, std::initializer_list<std::uint64_t> args, std::initializer_list<float> floatArgs);
// The d0 result of the last call() on this thread.
double lastFloatResult();
// Runs the thread's top-level function until it returns (thread entry / main).
std::uint64_t runThread(std::uint64_t function, std::initializer_list<std::uint64_t> args);

// The address of the stub holding SVC #kReturnSvc; set by the loader.
void setReturnStub(std::uint64_t address);
void setThreadExitStub(std::uint64_t address);
void invalidateCode(std::uint64_t address, std::size_t length);
void backtrace(const char *why);
} // namespace cpu
