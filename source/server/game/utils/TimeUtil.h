#pragma once
// Wall-clock time (unix epoch) and the daily shop cycle.
#include <cstdint>

namespace server {
namespace TimeUtil {
constexpr std::int64_t kMillisecondsPerSecond = 1000;
constexpr std::int64_t kMillisecondsPerDay = 86400000;
constexpr int kSecondsPerDay = 86400;

std::int64_t getMilliseconds();
std::int64_t getSeconds();

// The daily shop cycle length (--shop-period: 86400, shorter only for testing).
int getShopPeriodSeconds();
void setShopPeriodSeconds(int seconds);
} // namespace TimeUtil
} // namespace server
