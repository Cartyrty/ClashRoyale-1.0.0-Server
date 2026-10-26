#pragma once
// The main thread's run loop: window events, the main dispatch queue, posted calls, NSTimers and CADisplayLinks.
#include <cstdint>
#include <functional>

namespace runloop {
struct TimerInfo {
    double interval = 0;
    std::uint64_t target = 0, selector = 0, userInfo = 0, block = 0;
    bool repeats = false;
};
void defineTimer(std::uint64_t timer, const TimerInfo &info);
void addTimer(std::uint64_t timer);
void invalidateTimer(std::uint64_t timer);
bool timerValid(std::uint64_t timer);
std::uint64_t timerUserInfo(std::uint64_t timer);
double timerInterval(std::uint64_t timer);
void fireTimer(std::uint64_t timer);
// CADisplayLink: target/selector called once per frame (frameInterval frames).
void addDisplayLink(std::uint64_t link, std::uint64_t target, std::uint64_t selector, int frameInterval);
void setDisplayLinkInterval(std::uint64_t link, int frameInterval);
void removeDisplayLink(std::uint64_t link);
double displayLinkTimestamp();
// Calls from other threads into the main thread.
void post(std::function<void()> fn, bool wait = false);
void postAfter(double seconds, std::function<void()> fn);
// Called each iteration to process window events (set by the platform layer).
void setEventPump(std::function<void()> pump);
void runFor(double seconds);
[[noreturn]] void runForever();
} // namespace runloop
