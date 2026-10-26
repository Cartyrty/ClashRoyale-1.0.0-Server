// libSystem: blocks runtime, pthreads, OSAtomic, Grand Central Dispatch, process/dyld/sysctl queries.
#include "libc/System.h"
#include "Hle.h"
#include "Loader.h"
#include "Log.h"
#include "Memory.h"
#include "objc/ObjC.h"
#include "platform/Platform.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

// Guest words as host atomics (portable; replaces the MSVC interlocked intrinsics).
static std::atomic_ref<std::int32_t> atomic32(std::uint64_t a) { return std::atomic_ref<std::int32_t>(*reinterpret_cast<std::int32_t *>(mem::base() + a)); }
static std::atomic_ref<std::int64_t> atomic64(std::uint64_t a) { return std::atomic_ref<std::int64_t>(*reinterpret_cast<std::int64_t *>(mem::base() + a)); }

using hle::arg;
using hle::ret;

namespace sys {
namespace {
thread_local bool t_mainThread = false;
std::uint64_t g_stackClass = 0, g_mallocClass = 0, g_globalClass = 0;
std::uint64_t mallocBlockClass() {
    if (!g_mallocClass) g_mallocClass = objc::getClass("__NSMallocBlock__");
    return g_mallocClass;
}
} // namespace
void markMainThread() { t_mainThread = true; }
bool isMainThread() { return t_mainThread; }

void spawnGuestThread(const char *name, std::function<void()> fn) {
    std::string label = name ? name : "thread";
    std::thread([label, fn = std::move(fn)] {
        cpu::attachThread(mem::allocStack(1024 * 1024), 48 * 1024 * 1024);
        hlog("thread", "started %s", label.c_str());
        fn();
        hlog("thread", "finished %s", label.c_str());
    }).detach();
}

// --- Blocks: flags BLOCK_REFCOUNT_MASK 0xfffe, NEEDS_FREE 1<<24, HAS_COPY_DISPOSE 1<<25, IS_GLOBAL 1<<28.
// Descriptor: reserved +0, size +8, copy_helper +16, dispose_helper +24 (when HAS_COPY_DISPOSE). ---
std::uint64_t blockCopy(std::uint64_t block) {
    if (!block) return 0;
    const auto flags = mem::r32(block + 8);
    if (flags & (1u << 28)) return block; // global
    if (flags & (1u << 24)) {             // heap: bump the refcount
        atomic32(block + 8).fetch_add(2);
        return block;
    }
    const auto descriptor = mem::r64(block + 24);
    const auto size = mem::r64(descriptor + 8);
    const auto copy = mem::alloc(std::size_t(size));
    std::memcpy(mem::base() + copy, mem::base() + block, std::size_t(size));
    mem::w64(copy, mallocBlockClass());
    mem::w32(copy + 8, (flags & ~0xFFFEu) | (1u << 24) | 2);
    if (flags & (1u << 25)) cpu::call(mem::r64(descriptor + 16), {copy, block});
    return copy;
}
void blockRelease(std::uint64_t block) {
    if (!block) return;
    const auto flags = mem::r32(block + 8);
    if (!(flags & (1u << 24))) return;
    const std::int32_t before = atomic32(block + 8).fetch_add(-2);
    if ((before & 0xFFFE) == 2) {
        if (flags & (1u << 25)) cpu::call(mem::r64(mem::r64(block + 24) + 24), {block});
        // Freed memory is not reused (see NSObject -dealloc), so pending callers stay safe.
    }
}
std::uint64_t callBlock(std::uint64_t block, std::initializer_list<std::uint64_t> args) {
    std::vector<std::uint64_t> all{block};
    all.insert(all.end(), args.begin(), args.end());
    const auto invoke = mem::r64(block + 16);
    switch (all.size()) {
    case 1: return cpu::call(invoke, {all[0]});
    case 2: return cpu::call(invoke, {all[0], all[1]});
    case 3: return cpu::call(invoke, {all[0], all[1], all[2]});
    case 4: return cpu::call(invoke, {all[0], all[1], all[2], all[3]});
    default: return cpu::call(invoke, {all[0], all[1], all[2], all[3], all[4]});
    }
}
} // namespace sys

// Block classes (blocks are objects: -copy / -retain / -release work on them).
OBJC_CLASS(NSBlock, NSObject);
OBJC_CLASS(__NSStackBlock__, NSBlock);
OBJC_CLASS(__NSMallocBlock__, NSBlock);
OBJC_CLASS(__NSGlobalBlock__, NSBlock);
OBJC_METHOD(NSBlock, "copy", copy) { ret(sys::blockCopy(objc::self())); }
OBJC_METHOD(NSBlock, "copyWithZone:", copyWithZone) { ret(sys::blockCopy(objc::self())); }
OBJC_METHOD(NSBlock, "retain", retain_) { ret(sys::blockCopy(objc::self())); }
OBJC_METHOD(NSBlock, "release", release_) { sys::blockRelease(objc::self()); }
OBJC_METHOD(NSBlock, "autorelease", autorelease_) { ret(objc::self()); }
static hle::DataRegistrar d_stackBlock("__NSConcreteStackBlock", [] { return objc::getClass("__NSStackBlock__"); });
static hle::DataRegistrar d_globalBlock("__NSConcreteGlobalBlock", [] { return objc::getClass("__NSGlobalBlock__"); });
static hle::DataRegistrar d_mallocBlock("__NSConcreteMallocBlock", [] { return objc::getClass("__NSMallocBlock__"); });
HLE(_Block_copy) { ret(sys::blockCopy(arg(0))); }
HLE(_Block_release) { sys::blockRelease(arg(0)); }
HLE(objc_retainBlock) { ret(sys::blockCopy(arg(0))); }
// _Block_object_assign(dest, object, flags): BLOCK_FIELD_IS_OBJECT 3, IS_BLOCK 7, IS_BYREF 8, IS_WEAK 16, BYREF_CALLER 128.
HLE(_Block_object_assign) {
    const auto dest = arg(0), object = arg(1), flags = arg(2) & 0xFF;
    if ((flags & 0x80) || (flags & 31) == 3) { // object (or caller-side byref/object copies)
        if (!(flags & 0x80)) objc::retain(object);
        mem::w64(dest, object);
    } else if ((flags & 31) == 7) {
        mem::w64(dest, sys::blockCopy(object));
    } else if ((flags & 31) == 8 || (flags & 31) == 24) {
        // __block variable: Block_byref {isa, forwarding +8, flags +16, size +20, keep +24, destroy +32}.
        const auto src = object;
        const auto forwarding = mem::r64(src + 8);
        if (forwarding != src || (mem::r32(src + 16) & (1u << 24))) { // already on the heap
            const auto heap = forwarding;
            atomic32(heap + 16).fetch_add(2);
            mem::w64(dest, heap);
        } else {
            const auto size = mem::r32(src + 20);
            const auto heap = mem::alloc(size);
            std::memcpy(mem::base() + heap, mem::base() + src, size);
            mem::w32(heap + 16, (mem::r32(src + 16) & ~0xFFFEu) | (1u << 24) | 4);
            mem::w64(heap + 8, heap);
            mem::w64(src + 8, heap);
            if (mem::r32(src + 16) & (1u << 25)) cpu::call(mem::r64(src + 24), {heap, src});
            mem::w64(dest, heap);
        }
    } else {
        mem::w64(dest, object);
    }
}
HLE(_Block_object_dispose) {
    const auto object = arg(0), flags = arg(1) & 0xFF;
    if ((flags & 31) == 3) objc::release(object);
    else if ((flags & 31) == 7) sys::blockRelease(object);
    // byref copies are kept (never freed).
}

// --- pthreads ---
namespace {
std::mutex g_pthreadLock;
std::unordered_map<std::uint64_t, std::unique_ptr<std::recursive_mutex>> g_mutexes;
std::unordered_map<std::uint64_t, std::unique_ptr<std::condition_variable_any>> g_conds;
std::recursive_mutex &mutexFor(std::uint64_t address) {
    std::lock_guard lock(g_pthreadLock);
    auto &m = g_mutexes[address];
    if (!m) m = std::make_unique<std::recursive_mutex>();
    return *m;
}
std::condition_variable_any &condFor(std::uint64_t address) {
    std::lock_guard lock(g_pthreadLock);
    auto &c = g_conds[address];
    if (!c) c = std::make_unique<std::condition_variable_any>();
    return *c;
}
std::atomic<std::uint64_t> g_nextKey{1};
thread_local std::unordered_map<std::uint64_t, std::uint64_t> t_specific;
struct ThreadInfo {
    std::mutex lock;
    std::condition_variable done;
    bool finished = false;
    std::uint64_t result = 0;
};
std::unordered_map<std::uint64_t, std::shared_ptr<ThreadInfo>> g_threads;
std::atomic<std::uint32_t> g_machThreads{0x1003};
} // namespace
HLE(pthread_mutex_init) { mutexFor(arg(0)); mem::w64(arg(0), 0x32AAABA7); ret(0); }
HLE(pthread_mutex_destroy) { ret(0); }
HLE(pthread_mutex_lock) { mutexFor(arg(0)).lock(); ret(0); }
HLE(pthread_mutex_trylock) { ret(mutexFor(arg(0)).try_lock() ? 0 : 16); }
HLE(pthread_mutex_unlock) { mutexFor(arg(0)).unlock(); ret(0); }
HLE(pthread_mutexattr_init) { ret(0); }
HLE(pthread_mutexattr_settype) { ret(0); }
HLE(pthread_mutexattr_destroy) { ret(0); }
HLE(pthread_cond_init) { condFor(arg(0)); ret(0); }
HLE(pthread_cond_destroy) { ret(0); }
HLE(pthread_cond_signal) { condFor(arg(0)).notify_one(); ret(0); }
HLE(pthread_cond_broadcast) { condFor(arg(0)).notify_all(); ret(0); }
HLE(pthread_cond_wait) {
    auto &m = mutexFor(arg(1));
    condFor(arg(0)).wait(m);
    ret(0);
}
HLE(pthread_cond_timedwait) {
    auto &m = mutexFor(arg(1));
    const auto s = mem::r64(arg(2)), ns = mem::r64(arg(2) + 8);
    const auto deadline = std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::seconds(s) + std::chrono::nanoseconds(ns)));
    ret(condFor(arg(0)).wait_until(m, deadline) == std::cv_status::timeout ? 60 : 0);
}
HLE(pthread_key_create) { mem::w64(arg(0), g_nextKey++); ret(0); }
HLE(pthread_key_delete) { ret(0); }
HLE(pthread_getspecific) { auto f = t_specific.find(arg(0)); ret(f == t_specific.end() ? 0 : f->second); }
HLE(pthread_setspecific) { t_specific[arg(0)] = arg(1); ret(0); }
HLE(pthread_self) { ret(cpu::threadPointer()); }
HLE(pthread_equal) { ret(arg(0) == arg(1)); }
HLE(pthread_mach_thread_np) { ret(mem::r32(arg(0) + 0x100) ? mem::r32(arg(0) + 0x100) : 0x103); }
HLE(pthread_attr_init) { ret(0); }
HLE(pthread_attr_destroy) { ret(0); }
HLE(pthread_attr_setdetachstate) { ret(0); }
HLE(pthread_attr_setstacksize) { ret(0); }
HLE(pthread_attr_setschedparam) { ret(0); }
HLE(pthread_attr_getschedparam) { ret(0); }
HLE(pthread_setname_np) { ret(0); }
HLE(pthread_detach) { ret(0); }
HLE(pthread_once) {
    if (mem::r64(arg(0) + 8) == 1) return ret(0);
    std::lock_guard lock(mutexFor(arg(0)));
    if (mem::r64(arg(0) + 8) != 1) {
        cpu::call(arg(1));
        mem::w64(arg(0) + 8, 1);
    }
    ret(0);
}
// pthread_create(pthread_t *, attr, start, arg)
HLE(pthread_create) {
    const auto out = arg(0), start = arg(2), argument = arg(3);
    auto info = std::make_shared<ThreadInfo>();
    std::mutex ready;
    std::condition_variable readyCv;
    std::uint64_t handle = 0;
    std::thread([&, info, start, argument] {
        cpu::attachThread(mem::allocStack(1024 * 1024), 48 * 1024 * 1024);
        const auto self = cpu::threadPointer();
        mem::w32(self + 0x100, g_machThreads++);
        {
            std::lock_guard lock(g_pthreadLock);
            g_threads[self] = info;
        }
        {
            std::lock_guard lock(ready);
            handle = self;
        }
        readyCv.notify_one();
        hlog("thread", "pthread started at %s", loader::describe(start).c_str());
        const auto result = cpu::runThread(start, {argument});
        std::lock_guard lock(info->lock);
        info->finished = true;
        info->result = result;
        info->done.notify_all();
    }).detach();
    std::unique_lock lock(ready);
    readyCv.wait(lock, [&] { return handle != 0; });
    if (out) mem::w64(out, handle);
    ret(0);
}
HLE(pthread_join) {
    std::shared_ptr<ThreadInfo> info;
    {
        std::lock_guard lock(g_pthreadLock);
        auto f = g_threads.find(arg(0));
        if (f != g_threads.end()) info = f->second;
    }
    if (info) {
        std::unique_lock lock(info->lock);
        info->done.wait(lock, [&] { return info->finished; });
        if (arg(1)) mem::w64(arg(1), info->result);
    }
    ret(0);
}
HLE(pthread_exit) { cpu::backtrace("pthread_exit"); fatal("pthread_exit is not supported"); }
HLE(sched_yield) { std::this_thread::yield(); ret(0); }
HLE(mach_thread_self) { ret(0x103); }
HLE(pthread_threadid_np) { if (arg(1)) mem::w64(arg(1), platform::currentThreadId()); ret(0); }
HLE(pthread_main_np) { ret(sys::isMainThread()); }

// --- OSAtomic / spin locks (guest memory, host interlocked operations) ---
static std::int32_t i32(std::uint64_t v) { return std::int32_t(std::uint32_t(v)); }
static bool cas32(std::uint64_t a, std::uint64_t expected, std::uint64_t desired) {
    std::int32_t e = i32(expected);
    return atomic32(a).compare_exchange_strong(e, i32(desired));
}
static bool cas64(std::uint64_t a, std::uint64_t expected, std::uint64_t desired) {
    std::int64_t e = std::int64_t(expected);
    return atomic64(a).compare_exchange_strong(e, std::int64_t(desired));
}
HLE(OSAtomicAdd32) { ret(std::uint32_t(atomic32(arg(1)).fetch_add(i32(arg(0))) + i32(arg(0)))); }
HLE(OSAtomicAdd32Barrier) { ret(std::uint32_t(atomic32(arg(1)).fetch_add(i32(arg(0))) + i32(arg(0)))); }
HLE(OSAtomicIncrement32) { ret(std::uint32_t(atomic32(arg(0)).fetch_add(1) + 1)); }
HLE(OSAtomicIncrement32Barrier) { ret(std::uint32_t(atomic32(arg(0)).fetch_add(1) + 1)); }
HLE(OSAtomicDecrement32) { ret(std::uint32_t(atomic32(arg(0)).fetch_sub(1) - 1)); }
HLE(OSAtomicDecrement32Barrier) { ret(std::uint32_t(atomic32(arg(0)).fetch_sub(1) - 1)); }
HLE(OSAtomicAdd64) { ret(std::uint64_t(atomic64(arg(1)).fetch_add(std::int64_t(arg(0))) + std::int64_t(arg(0)))); }
HLE(OSAtomicAdd64Barrier) { ret(std::uint64_t(atomic64(arg(1)).fetch_add(std::int64_t(arg(0))) + std::int64_t(arg(0)))); }
HLE(OSAtomicCompareAndSwap32) { ret(cas32(arg(2), arg(0), arg(1))); }
HLE(OSAtomicCompareAndSwap32Barrier) { ret(cas32(arg(2), arg(0), arg(1))); }
HLE(OSAtomicCompareAndSwapIntBarrier) { ret(cas32(arg(2), arg(0), arg(1))); }
// Pointers and longs are 64-bit on arm64 iOS.
HLE(OSAtomicCompareAndSwapPtrBarrier) { ret(cas64(arg(2), arg(0), arg(1))); }
HLE(OSAtomicCompareAndSwapLongBarrier) { ret(cas64(arg(2), arg(0), arg(1))); }
HLE(OSAtomicCompareAndSwap64Barrier) { ret(cas64(arg(2), arg(0), arg(1))); }
HLE(OSMemoryBarrier) { std::atomic_thread_fence(std::memory_order_seq_cst); }
HLE(OSSpinLockLock) { while (!cas32(arg(0), 0, 1)) std::this_thread::yield(); }
HLE(OSSpinLockTry) { ret(cas32(arg(0), 0, 1)); }
HLE(OSSpinLockUnlock) { atomic32(arg(0)).store(0); }

// --- Grand Central Dispatch ---
namespace {
struct Task {
    std::uint64_t block = 0, function = 0, context = 0;
    std::shared_ptr<std::promise<void>> done;
    std::uint64_t group = 0; // dispatch_group_async: leave this group afterwards
};
void groupLeave(std::uint64_t group);
void runTask(const Task &t) {
    if (t.block) {
        sys::callBlock(t.block);
        sys::blockRelease(t.block);
    } else if (t.function) {
        cpu::call(t.function, {t.context});
    }
    if (t.done) t.done->set_value();
    if (t.group) groupLeave(t.group);
}
struct Queue {
    std::string label;
    bool main = false, concurrent = false;
    std::mutex lock;
    std::condition_variable wake;
    std::deque<Task> tasks;
    std::recursive_mutex exclusive; // held while a task of a serial queue runs (dispatch_sync on it takes it too)
    int workers = 0;
};
std::mutex g_queuesLock;
std::unordered_map<std::uint64_t, std::shared_ptr<Queue>> g_queues;
std::uint64_t g_mainQueue = 0;
std::shared_ptr<Queue> queueFor(std::uint64_t q) {
    std::lock_guard lock(g_queuesLock);
    auto &e = g_queues[q];
    if (!e) e = std::make_shared<Queue>();
    return e;
}
std::uint64_t newQueueObject(const std::string &label, bool concurrent, bool main = false) {
    const auto object = mem::hleAlloc(128, 16);
    mem::w64(object, objc::getClass("OS_dispatch_queue"));
    mem::w64(object + 72, mem::hleString(label)); // dq_label
    auto q = queueFor(object);
    q->label = label;
    q->concurrent = concurrent;
    q->main = main;
    return object;
}
std::uint64_t mainQueue() {
    if (!g_mainQueue) g_mainQueue = newQueueObject("com.apple.main-thread", false, true);
    return g_mainQueue;
}
std::uint64_t g_globalQueue = 0;
std::uint64_t globalQueue() {
    if (!g_globalQueue) g_globalQueue = newQueueObject("com.apple.root.default-qos", true);
    return g_globalQueue;
}
void startWorker(std::shared_ptr<Queue> q) {
    ++q->workers;
    sys::spawnGuestThread(q->label.c_str(), [q] {
        for (;;) {
            Task t;
            {
                std::unique_lock lock(q->lock);
                q->wake.wait(lock, [&] { return !q->tasks.empty(); });
                t = q->tasks.front();
                q->tasks.pop_front();
            }
            if (q->concurrent) runTask(t);
            else {
                std::lock_guard exclusive(q->exclusive);
                runTask(t);
            }
        }
    });
}
void enqueue(std::uint64_t queueObject, Task t) {
    auto q = queueFor(queueObject ? queueObject : globalQueue());
    {
        std::lock_guard lock(q->lock);
        q->tasks.push_back(t);
    }
    if (q->main) return; // drained by the main run loop
    {
        std::lock_guard lock(q->lock);
        const int wanted = q->concurrent ? 4 : 1;
        if (q->workers < wanted && q->tasks.size() > 0) {
            q->lock.unlock();
            startWorker(q);
            q->lock.lock();
        }
    }
    q->wake.notify_one();
}
// Delayed tasks.
struct Timed {
    std::chrono::steady_clock::time_point when;
    std::uint64_t queue;
    Task task;
};
std::mutex g_timerLock;
std::condition_variable g_timerWake;
std::vector<Timed> g_timed;
bool g_timerThread = false;
void scheduleAfter(std::chrono::steady_clock::time_point when, std::uint64_t queue, Task task) {
    std::lock_guard lock(g_timerLock);
    g_timed.push_back({when, queue, task});
    if (!g_timerThread) {
        g_timerThread = true;
        std::thread([] {
            for (;;) {
                std::unique_lock lock(g_timerLock);
                if (g_timed.empty()) { g_timerWake.wait(lock); continue; }
                auto next = std::min_element(g_timed.begin(), g_timed.end(), [](auto &a, auto &b) { return a.when < b.when; });
                if (next->when > std::chrono::steady_clock::now()) { g_timerWake.wait_until(lock, next->when); continue; }
                const Timed t = *next;
                g_timed.erase(next);
                lock.unlock();
                enqueue(t.queue, t.task);
            }
        }).detach();
    }
    g_timerWake.notify_one();
}
// dispatch_time values: nanoseconds on the mach_absolute_time scale; DISPATCH_TIME_NOW 0, FOREVER ~0.
const auto g_epoch = std::chrono::steady_clock::now();
std::uint64_t nowNanos() { return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - g_epoch).count()); }
std::chrono::steady_clock::time_point toTimePoint(std::uint64_t t) { return g_epoch + std::chrono::nanoseconds(t); }
} // namespace
void sys::drainMainQueue() {
    auto q = queueFor(mainQueue());
    for (int i = 0; i < 64; ++i) {
        Task t;
        {
            std::lock_guard lock(q->lock);
            if (q->tasks.empty()) return;
            t = q->tasks.front();
            q->tasks.pop_front();
        }
        runTask(t);
    }
}
OBJC_CLASS(OS_dispatch_object, NSObject);
OBJC_CLASS(OS_dispatch_queue, OS_dispatch_object);
OBJC_CLASS(OS_dispatch_semaphore, OS_dispatch_object);
OBJC_CLASS(OS_dispatch_group, OS_dispatch_object);
static hle::DataRegistrar d_mainQueue("__dispatch_main_q", [] { return mainQueue(); });
HLE(dispatch_get_main_queue) { ret(mainQueue()); }
HLE(dispatch_get_global_queue) { ret(globalQueue()); }
HLE(dispatch_queue_create) {
    const std::string label = arg(0) ? mem::cstring(arg(0)) : "queue";
    ret(newQueueObject(label, arg(1) != 0));
}
HLE(dispatch_queue_get_label) { ret(mem::r64((arg(0) ? arg(0) : mainQueue()) + 72)); }
HLE(dispatch_set_target_queue) {}
HLE(dispatch_retain) {}
HLE(dispatch_release) {}
HLE(dispatch_async) { enqueue(arg(0), {sys::blockCopy(arg(1))}); }
HLE(dispatch_async_f) { enqueue(arg(0), {0, arg(2), arg(1)}); }
HLE(dispatch_barrier_async) { enqueue(arg(0), {sys::blockCopy(arg(1))}); }
static void syncRun(std::uint64_t queueObject, Task t) {
    auto q = queueFor(queueObject ? queueObject : globalQueue());
    if (q->main && !sys::isMainThread()) {
        t.done = std::make_shared<std::promise<void>>();
        auto f = t.done->get_future();
        if (t.block) t.block = sys::blockCopy(t.block);
        enqueue(queueObject, t);
        f.wait();
        return;
    }
    if (q->concurrent || q->main) { runTask(t); return; }
    std::lock_guard exclusive(q->exclusive);
    if (t.block) { sys::callBlock(t.block); return; }
    runTask(t);
}
HLE(dispatch_sync) { syncRun(arg(0), {arg(1)}); }
HLE(dispatch_sync_f) { syncRun(arg(0), {0, arg(2), arg(1)}); }
HLE(dispatch_barrier_sync) { syncRun(arg(0), {arg(1)}); }
HLE(dispatch_after) { scheduleAfter(toTimePoint(arg(0)), arg(1), {sys::blockCopy(arg(2))}); }
HLE(dispatch_after_f) { scheduleAfter(toTimePoint(arg(0)), arg(1), {0, arg(3), arg(2)}); }
HLE(dispatch_time) {
    const auto base = arg(0) == 0 ? nowNanos() : arg(0);
    if (arg(0) == ~0ull) return ret(~0ull);
    ret(std::uint64_t(std::int64_t(base) + std::int64_t(arg(1))));
}
HLE(dispatch_walltime) { ret(nowNanos() + arg(1)); }
// dispatch_once(predicate, block): ~0 = done.
namespace { std::recursive_mutex g_onceLock; }
HLE(dispatch_once) {
    if (mem::r64(arg(0)) == ~0ull) return;
    std::lock_guard lock(g_onceLock);
    if (mem::r64(arg(0)) == ~0ull) return;
    sys::callBlock(arg(1));
    mem::w64(arg(0), ~0ull);
}
HLE(dispatch_once_f) {
    if (mem::r64(arg(0)) == ~0ull) return;
    std::lock_guard lock(g_onceLock);
    if (mem::r64(arg(0)) == ~0ull) return;
    cpu::call(arg(2), {arg(1)});
    mem::w64(arg(0), ~0ull);
}
// Semaphores and groups.
namespace {
struct Semaphore {
    std::mutex lock;
    std::condition_variable cv;
    std::int64_t value = 0;
};
struct Group {
    std::mutex lock;
    std::condition_variable cv;
    std::int64_t count = 0;
    std::vector<std::pair<std::uint64_t, Task>> notify;
};
std::mutex g_syncObjectsLock;
std::unordered_map<std::uint64_t, std::shared_ptr<Semaphore>> g_semaphores;
std::unordered_map<std::uint64_t, std::shared_ptr<Group>> g_groups;
std::shared_ptr<Semaphore> semaphore(std::uint64_t o) { std::lock_guard l(g_syncObjectsLock); return g_semaphores[o]; }
std::shared_ptr<Group> group(std::uint64_t o) { std::lock_guard l(g_syncObjectsLock); return g_groups[o]; }
} // namespace
HLE(dispatch_semaphore_create) {
    const auto o = mem::hleAlloc(32, 16);
    mem::w64(o, objc::getClass("OS_dispatch_semaphore"));
    auto s = std::make_shared<Semaphore>();
    s->value = std::int64_t(arg(0));
    std::lock_guard l(g_syncObjectsLock);
    g_semaphores[o] = s;
    ret(o);
}
HLE(dispatch_semaphore_signal) {
    auto s = semaphore(arg(0));
    if (!s) return ret(0);
    std::lock_guard l(s->lock);
    ++s->value;
    s->cv.notify_one();
    ret(0);
}
HLE(dispatch_semaphore_wait) {
    auto s = semaphore(arg(0));
    if (!s) return ret(0);
    std::unique_lock l(s->lock);
    const auto timeout = arg(1);
    if (timeout == ~0ull) s->cv.wait(l, [&] { return s->value > 0; });
    else if (!s->cv.wait_until(l, toTimePoint(timeout == 0 ? nowNanos() : timeout), [&] { return s->value > 0; })) return ret(49);
    --s->value;
    ret(0);
}
HLE(dispatch_group_create) {
    const auto o = mem::hleAlloc(32, 16);
    mem::w64(o, objc::getClass("OS_dispatch_group"));
    std::lock_guard l(g_syncObjectsLock);
    g_groups[o] = std::make_shared<Group>();
    ret(o);
}
HLE(dispatch_group_enter) { auto g = group(arg(0)); std::lock_guard l(g->lock); ++g->count; }
namespace {
void groupLeave(std::uint64_t o) {
    auto g = group(o);
    std::vector<std::pair<std::uint64_t, Task>> fire;
    {
        std::lock_guard l(g->lock);
        if (--g->count > 0) return;
        g->count = 0;
        fire.swap(g->notify);
        g->cv.notify_all();
    }
    for (auto &[q, t] : fire) enqueue(q, t);
}
} // namespace
HLE(dispatch_group_leave) { groupLeave(arg(0)); }
HLE(dispatch_group_notify) {
    auto g = group(arg(0));
    {
        std::lock_guard l(g->lock);
        if (g->count > 0) { g->notify.push_back({arg(1), {sys::blockCopy(arg(2))}}); return; }
    }
    enqueue(arg(1), {sys::blockCopy(arg(2))});
}
HLE(dispatch_group_wait) {
    auto g = group(arg(0));
    std::unique_lock l(g->lock);
    if (arg(1) == ~0ull) g->cv.wait(l, [&] { return g->count <= 0; });
    else if (!g->cv.wait_until(l, toTimePoint(arg(1)), [&] { return g->count <= 0; })) return ret(49);
    ret(0);
}
HLE(dispatch_group_async) {
    const auto g = arg(0);
    { auto gr = group(g); std::lock_guard l(gr->lock); ++gr->count; }
    Task t{sys::blockCopy(arg(2))};
    t.group = g;
    enqueue(arg(1), t);
}
HLE(dispatch_main) { cpu::backtrace("dispatch_main"); fatal("dispatch_main is not supported"); }

// --- Process, dyld and system queries ---
HLE(getpid) { ret(4242); }
HLE(getppid) { ret(1); }
HLE(getuid) { ret(501); }
HLE(geteuid) { ret(501); }
HLE(getgid) { ret(501); }
HLE(getprogname) { static std::uint64_t name = mem::hleString("Clash Royale"); ret(name); }
HLE(_NSGetExecutablePath) {
    const std::string path = "/var/containers/Bundle/Application/5C3A39B1-6A3B-4C2E-9D5A-0C1A2B3C4D5E/Clash Royale.app/Clash Royale";
    if (mem::r32(arg(1)) < path.size() + 1) { mem::w32(arg(1), std::uint32_t(path.size() + 1)); return ret(~0ull); }
    std::memcpy(mem::base() + arg(0), path.c_str(), path.size() + 1);
    ret(0);
}
// The jailbreak check in -[AppController application:didFinishLaunchingWithOptions:] hashes every loaded image
// name and calls system(NULL); a stock device reports its images and no shell. Only the app image is reported.
HLE(_dyld_image_count) { ret(1); }
HLE(_dyld_get_image_header) { ret(arg(0) == 0 ? loader::imageBase() : 0); }
HLE(_dyld_get_image_name) {
    static std::uint64_t name = mem::hleString("/var/containers/Bundle/Application/5C3A39B1-6A3B-4C2E-9D5A-0C1A2B3C4D5E/Clash Royale.app/Clash Royale");
    ret(arg(0) == 0 ? name : 0);
}
HLE(_dyld_get_image_vmaddr_slide) { ret(0); }
HLE(_dyld_register_func_for_add_image) {}
HLE(_dyld_register_func_for_remove_image) {}
HLE(dladdr) { ret(0); }
HLE(dlopen) { ret(0); }
HLE(dlsym) {
    const auto name = "_" + mem::cstring(arg(1));
    if (const auto local = loader::symbol(name)) return ret(local);
    // Framework symbols resolve like imports: variables to their storage, functions to trap stubs.
    hlog("dlsym", "%s", name.c_str());
    ret(hle::resolveImport(name));
}
HLE(dlerror) { ret(0); }
HLE(getsectdatafromheader) { ret(0); }
HLE(getsectdatafromheader_64) { ret(0); }
HLE(NSVersionOfLinkTimeLibrary) { ret(~0ull); }
// sysctl / sysctlbyname: hw.machine "iPhone7,2", hw.model "N61AP", hw.ncpu 2, hw.memsize 1 GB, kern.boottime.
static bool sysctlValue(const std::string &name, std::string &out, std::uint64_t &number, bool &isNumber) {
    isNumber = false;
    if (name == "hw.machine") { out = "iPhone7,2"; return true; }
    if (name == "hw.model") { out = "N61AP"; return true; }
    if (name == "kern.osversion") { out = "13G36"; return true; }
    if (name == "kern.osrelease") { out = "15.6.0"; return true; }
    if (name == "kern.hostname") { out = "iPhone"; return true; }
    isNumber = true;
    if (name == "hw.ncpu" || name == "hw.activecpu" || name == "hw.physicalcpu" || name == "hw.logicalcpu") { number = 2; return true; }
    if (name == "hw.memsize" || name == "hw.physmem") { number = 1ull << 30; return true; }
    if (name == "hw.cpufrequency") { number = 1400000000; return true; }
    if (name == "hw.cputype") { number = 0x0100000C; return true; }
    if (name == "hw.cpusubtype") { number = 1; return true; }
    return false;
}
static void sysctlOut(const std::string &name, std::uint64_t oldp, std::uint64_t oldlenp) {
    std::string s;
    std::uint64_t n = 0;
    bool isNumber;
    if (!sysctlValue(name, s, n, isNumber)) {
        hlog("sysctl", "unknown %s", name.c_str());
        return hle::ret(~0ull);
    }
    if (isNumber) {
        const auto len = oldlenp ? mem::r64(oldlenp) : 8;
        if (oldp) { if (len >= 8) mem::w64(oldp, n); else mem::w32(oldp, std::uint32_t(n)); }
        if (oldlenp) mem::w64(oldlenp, len >= 8 ? 8 : 4);
    } else {
        if (oldp && oldlenp && mem::r64(oldlenp) >= s.size() + 1) std::memcpy(mem::base() + oldp, s.c_str(), s.size() + 1);
        if (oldlenp) mem::w64(oldlenp, s.size() + 1);
    }
    hle::ret(0);
}
HLE(sysctlbyname) { sysctlOut(mem::cstring(arg(0)), arg(1), arg(2)); }
HLE(sysctl) {
    // CTL_HW 6: HW_MACHINE 1, HW_MODEL 2, HW_NCPU 3, HW_MEMSIZE 24; CTL_KERN 1: KERN_PROC 14 (debugger check).
    const auto a = mem::r32(arg(0)), b = arg(1) > 1 ? mem::r32(arg(0) + 4) : 0;
    std::string name;
    if (a == 6 && b == 1) name = "hw.machine";
    else if (a == 6 && b == 2) name = "hw.model";
    else if (a == 6 && b == 3) name = "hw.ncpu";
    else if (a == 6 && b == 24) name = "hw.memsize";
    else if (a == 1 && b == 14) {
        // kinfo_proc: zero-filled (p_flag without P_TRACED: no debugger attached).
        if (arg(2) && arg(3)) std::memset(mem::base() + arg(2), 0, std::size_t(mem::r64(arg(3))));
        return ret(0);
    }
    if (name.empty()) { hlog("sysctl", "unknown mib %u.%u", a, unsigned(b)); return ret(~0ull); }
    sysctlOut(name, arg(2), arg(3));
}
// struct utsname: 5 fields of 256 bytes.
HLE(uname) {
    const char *fields[5] = {"Darwin", "iPhone", "15.6.0", "Darwin Kernel Version 15.6.0", "iPhone7,2"};
    for (int i = 0; i < 5; ++i) {
        std::memset(mem::base() + arg(0) + 256 * std::uint64_t(i), 0, 256);
        std::strcpy(reinterpret_cast<char *>(mem::base() + arg(0) + 256 * std::uint64_t(i)), fields[i]);
    }
    ret(0);
}
HLE(gethostname) { std::strcpy(reinterpret_cast<char *>(mem::base() + arg(0)), "iPhone"); ret(0); }
static hle::DataRegistrar d_pageSize("_vm_page_size", [] { const auto p = mem::hleAlloc(8); mem::w64(p, 16384); return p; });
static hle::DataRegistrar d_pageMask("_vm_page_mask", [] { const auto p = mem::hleAlloc(8); mem::w64(p, 16383); return p; });
static hle::DataRegistrar d_taskSelf("_mach_task_self_", [] { const auto p = mem::hleAlloc(8); mem::w32(p, 0x103); return p; });
// Mach and exception-port calls (crash reporters): report failure so they stay inactive.
HLE(task_get_exception_ports) { ret(5); }
HLE(task_swap_exception_ports) { ret(5); }
HLE(thread_get_exception_ports) { ret(5); }
HLE(thread_swap_exception_ports) { ret(5); }
HLE(mach_port_allocate) { ret(5); }
HLE(mach_port_deallocate) { ret(0); }
HLE(mach_port_insert_right) { ret(5); }
HLE(mach_port_mod_refs) { ret(0); }
HLE(mach_port_move_member) { ret(5); }
HLE(mach_port_request_notification) { ret(5); }
HLE(mach_msg) { ret(0x10004003); }
HLE(task_threads) { ret(5); }
HLE(thread_get_state) { ret(5); }
HLE(thread_suspend) { ret(0); }
HLE(thread_resume) { ret(0); }
HLE(vm_allocate) {
    const auto block = mem::calloc(std::size_t(arg(2) + 0x4000));
    mem::w64(arg(1), (block + 0x3FFF) & ~0x3FFFull);
    ret(0);
}
HLE(vm_deallocate) { ret(0); }
HLE(vm_read_overwrite) {
    if (!mem::valid(arg(1), arg(2))) return ret(1);
    std::memmove(mem::base() + arg(3), mem::base() + arg(1), std::size_t(arg(2)));
    if (arg(4)) mem::w64(arg(4), arg(2));
    ret(0);
}
HLE(vm_map) { ret(5); }
HLE(mach_make_memory_entry_64) { ret(5); }
HLE(exception_raise) { ret(5); }
HLE(exception_raise_state) { ret(5); }
HLE(exception_raise_state_identity) { ret(5); }
// Apple System Log queries (used to read crash logs): nothing found.
HLE(asl_new) { ret(mem::calloc(16)); }
HLE(asl_set_query) { ret(0); }
HLE(asl_search) { ret(0); }
HLE(aslresponse_next) { ret(0); }
HLE(aslresponse_free) {}
HLE(asl_free) {}
HLE(asl_release) {}
HLE(asl_get) { ret(0); }
HLE(asl_key) { ret(0); }
HLE(getifaddrs) { mem::w64(arg(0), 0); ret(0); }
HLE(freeifaddrs) {}
HLE(if_nametoindex) { ret(0); }
