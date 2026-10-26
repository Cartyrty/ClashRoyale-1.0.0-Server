// libSystem: clocks, sleeping, calendar time.
#include "Hle.h"
#include "Memory.h"
#include "platform/Platform.h"
#include "platform/Platform.h"
#include <chrono>
#include <ctime>
#include <thread>

using hle::arg;
using hle::ret;
namespace {
const auto g_start = std::chrono::steady_clock::now();
std::int64_t nanosSinceStart() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - g_start).count();
}
std::int64_t unixMicros() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
// struct tm (Darwin): 9 ints, then long tm_gmtoff (+40), char *tm_zone (+48).
void writeTm(std::uint64_t out, const std::tm &t, long gmtoff) {
    const int fields[9] = {t.tm_sec, t.tm_min, t.tm_hour, t.tm_mday, t.tm_mon, t.tm_year, t.tm_wday, t.tm_yday, t.tm_isdst};
    for (int i = 0; i < 9; ++i) mem::w32(out + 4 * std::uint64_t(i), std::uint32_t(fields[i]));
    mem::w64(out + 40, std::uint64_t(std::int64_t(gmtoff)));
    static std::uint64_t utc = mem::hleString("UTC");
    mem::w64(out + 48, utc);
}
std::tm readTm(std::uint64_t in) {
    std::tm t{};
    int *fields[9] = {&t.tm_sec, &t.tm_min, &t.tm_hour, &t.tm_mday, &t.tm_mon, &t.tm_year, &t.tm_wday, &t.tm_yday, &t.tm_isdst};
    for (int i = 0; i < 9; ++i) *fields[i] = std::int32_t(mem::r32(in + 4 * std::uint64_t(i)));
    return t;
}
} // namespace
// The device timebase: 24 MHz ticks, mach_timebase_info = 125/3 (ns per tick).
HLE(mach_absolute_time) { ret(std::uint64_t(nanosSinceStart() * 3 / 125)); }
HLE(mach_timebase_info) {
    mem::w32(arg(0), 125);
    mem::w32(arg(0) + 4, 3);
    ret(0);
}
HLE(gettimeofday) {
    const auto us = unixMicros();
    if (arg(0)) {
        mem::w64(arg(0), std::uint64_t(us / 1000000));
        mem::w32(arg(0) + 8, std::uint32_t(us % 1000000));
    }
    ret(0);
}
HLE(time) {
    const auto s = std::uint64_t(unixMicros() / 1000000);
    if (arg(0)) mem::w64(arg(0), s);
    ret(s);
}
HLE(clock_gettime) {
    // CLOCK_REALTIME 0, CLOCK_MONOTONIC 6 on Darwin.
    std::int64_t ns = arg(0) == 0 ? unixMicros() * 1000 : nanosSinceStart();
    mem::w64(arg(1), std::uint64_t(ns / 1000000000));
    mem::w64(arg(1) + 8, std::uint64_t(ns % 1000000000));
    ret(0);
}
HLE(clock) { ret(std::uint64_t(nanosSinceStart() / 1000)); }
HLE(usleep) { std::this_thread::sleep_for(std::chrono::microseconds(arg(0))); ret(0); }
HLE(sleep) { std::this_thread::sleep_for(std::chrono::seconds(arg(0))); ret(0); }
HLE(nanosleep) {
    const auto s = mem::r64(arg(0)), ns = mem::r64(arg(0) + 8);
    std::this_thread::sleep_for(std::chrono::seconds(s) + std::chrono::nanoseconds(ns));
    ret(0);
}
HLE(localtime_r) {
    const std::time_t t = std::time_t(mem::r64(arg(0)));
    const std::tm out = platform::localTime(t);
    writeTm(arg(1), out, platform::utcOffsetSeconds());
    ret(arg(1));
}
HLE(gmtime_r) {
    const std::time_t t = std::time_t(mem::r64(arg(0)));
    const std::tm out = platform::utcTime(t);
    writeTm(arg(1), out, 0);
    ret(arg(1));
}
HLE(localtime) {
    static std::uint64_t buffer = mem::hleAlloc(64);
    const std::time_t t = std::time_t(mem::r64(arg(0)));
    const std::tm out = platform::localTime(t);
    writeTm(buffer, out, 0);
    ret(buffer);
}
HLE(gmtime) {
    static std::uint64_t buffer = mem::hleAlloc(64);
    const std::time_t t = std::time_t(mem::r64(arg(0)));
    const std::tm out = platform::utcTime(t);
    writeTm(buffer, out, 0);
    ret(buffer);
}
HLE(mktime) {
    std::tm t = readTm(arg(0));
    ret(std::uint64_t(std::mktime(&t)));
}
HLE(timegm) {
    std::tm t = readTm(arg(0));
    ret(std::uint64_t(platform::fromUtcTime(t)));
}
HLE(strftime) {
    const std::tm t = readTm(arg(3));
    char buffer[1024];
    const auto n = std::strftime(buffer, sizeof buffer, mem::cstring(arg(2)).c_str(), &t);
    const auto max = std::size_t(arg(1));
    if (n >= max) return ret(0);
    std::memcpy(mem::base() + arg(0), buffer, n + 1);
    ret(n);
}
HLE(difftime) { hle::retDouble(double(std::int64_t(arg(0)) - std::int64_t(arg(1)))); }
