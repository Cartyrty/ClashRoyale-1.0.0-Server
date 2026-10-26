#include "foundation/RunLoop.h"
#include "Log.h"
#include "Memory.h"
#include "libc/System.h"
#include "objc/ObjC.h"
#include "platform/Platform.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace runloop {
namespace {
using Clock = std::chrono::steady_clock;
struct Timer {
    TimerInfo info;
    bool valid = true, scheduled = false;
    Clock::time_point next;
};
struct DisplayLink {
    std::uint64_t target, selector;
    int interval = 1, counter = 0;
};
std::recursive_mutex g_lock;
std::map<std::uint64_t, Timer> g_timers;
std::map<std::uint64_t, DisplayLink> g_links;
std::vector<std::pair<Clock::time_point, std::function<void()>>> g_posted;
std::function<void()> &pump() { static std::function<void()> f; return f; } // set from other files
Clock::time_point g_nextFrame = Clock::now();
double g_frameTimestamp = 0;
constexpr auto kFrame = std::chrono::nanoseconds(16666667);

void runTimer(std::uint64_t timer, const TimerInfo &info) {
    if (info.block) sys::callBlock(info.block, {timer});
    else if (info.target) objc::send(info.target, objc::selName(info.selector), {timer});
}
void iterate() {
    if (pump()) pump()();
    sys::drainMainQueue();
    // Posted calls that are due.
    std::vector<std::function<void()>> due;
    {
        std::lock_guard lock(g_lock);
        const auto now = Clock::now();
        for (auto it = g_posted.begin(); it != g_posted.end();) {
            if (it->first <= now) { due.push_back(std::move(it->second)); it = g_posted.erase(it); }
            else ++it;
        }
    }
    for (auto &fn : due) fn();
    // Timers.
    std::vector<std::pair<std::uint64_t, TimerInfo>> fire;
    {
        std::lock_guard lock(g_lock);
        const auto now = Clock::now();
        for (auto &[t, timer] : g_timers) {
            if (!timer.valid || !timer.scheduled || timer.next > now) continue;
            fire.push_back({t, timer.info});
            if (timer.info.repeats) {
                const auto step = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(timer.info.interval));
                timer.next += step;
                if (timer.next < now) timer.next = now + step;
            } else timer.valid = false;
        }
    }
    for (auto &[t, info] : fire) runTimer(t, info);
    // Display links at 60 Hz.
    const auto now = Clock::now();
    if (now >= g_nextFrame) {
        g_nextFrame += kFrame;
        if (g_nextFrame < now) g_nextFrame = now + kFrame;
        g_frameTimestamp = std::chrono::duration<double>(now.time_since_epoch()).count();
        std::vector<std::pair<std::uint64_t, DisplayLink>> links;
        {
            std::lock_guard lock(g_lock);
            for (auto &[l, link] : g_links)
                if (++link.counter >= link.interval) { link.counter = 0; links.push_back({l, link}); }
        }
        for (auto &[l, link] : links) objc::send(link.target, objc::selName(link.selector), {l});
    }
}
Clock::time_point nextWake() {
    std::lock_guard lock(g_lock);
    auto wake = g_nextFrame;
    for (auto &[t, timer] : g_timers) if (timer.valid && timer.scheduled && timer.next < wake) wake = timer.next;
    for (auto &[when, fn] : g_posted) if (when < wake) wake = when;
    return wake;
}
void sleepUntil(Clock::time_point when) {
    const auto now = Clock::now();
    if (when <= now) return;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(when - now).count();
    // Wake early for window input.
    platform::waitForEvents(int(std::max<long long>(0, ms - 1)));
    while (Clock::now() < when) std::this_thread::yield();
}
} // namespace

void defineTimer(std::uint64_t timer, const TimerInfo &info) {
    std::lock_guard lock(g_lock);
    g_timers[timer].info = info;
}
void addTimer(std::uint64_t timer) {
    std::lock_guard lock(g_lock);
    auto &t = g_timers[timer];
    t.scheduled = true;
    t.valid = true;
    t.next = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(t.info.interval));
}
void invalidateTimer(std::uint64_t timer) {
    std::lock_guard lock(g_lock);
    if (auto f = g_timers.find(timer); f != g_timers.end()) f->second.valid = false;
}
bool timerValid(std::uint64_t timer) {
    std::lock_guard lock(g_lock);
    auto f = g_timers.find(timer);
    return f != g_timers.end() && f->second.valid;
}
std::uint64_t timerUserInfo(std::uint64_t timer) {
    std::lock_guard lock(g_lock);
    auto f = g_timers.find(timer);
    return f == g_timers.end() ? 0 : f->second.info.userInfo;
}
double timerInterval(std::uint64_t timer) {
    std::lock_guard lock(g_lock);
    auto f = g_timers.find(timer);
    return f == g_timers.end() ? 0 : f->second.info.interval;
}
void fireTimer(std::uint64_t timer) {
    TimerInfo info;
    {
        std::lock_guard lock(g_lock);
        auto f = g_timers.find(timer);
        if (f == g_timers.end()) return;
        info = f->second.info;
    }
    runTimer(timer, info);
}
void addDisplayLink(std::uint64_t link, std::uint64_t target, std::uint64_t selector, int frameInterval) {
    std::lock_guard lock(g_lock);
    g_links[link] = {target, selector, std::max(1, frameInterval), 0};
}
void setDisplayLinkInterval(std::uint64_t link, int frameInterval) {
    std::lock_guard lock(g_lock);
    if (auto f = g_links.find(link); f != g_links.end()) f->second.interval = std::max(1, frameInterval);
}
void removeDisplayLink(std::uint64_t link) {
    std::lock_guard lock(g_lock);
    g_links.erase(link);
}
double displayLinkTimestamp() { return g_frameTimestamp; }
void post(std::function<void()> fn, bool wait) {
    if (!wait) {
        std::lock_guard lock(g_lock);
        g_posted.push_back({Clock::now(), std::move(fn)});
        return;
    }
    auto done = std::make_shared<std::promise<void>>();
    auto future = done->get_future();
    {
        std::lock_guard lock(g_lock);
        g_posted.push_back({Clock::now(), [fn = std::move(fn), done] { fn(); done->set_value(); }});
    }
    future.wait();
}
void postAfter(double seconds, std::function<void()> fn) {
    std::lock_guard lock(g_lock);
    g_posted.push_back({Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds)), std::move(fn)});
}
void setEventPump(std::function<void()> eventPump) { runloop::pump() = std::move(eventPump); }
void runFor(double seconds) {
    const auto end = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(std::max(0.0, seconds)));
    do {
        iterate();
        const auto wake = std::min(nextWake(), end);
        sleepUntil(wake);
    } while (Clock::now() < end);
}
void runForever() {
    hlog("runloop", "main run loop started");
    for (;;) {
        iterate();
        sleepUntil(nextWake());
    }
}
} // namespace runloop
