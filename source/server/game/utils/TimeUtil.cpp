#include "game/utils/TimeUtil.h"

#include <chrono>

namespace server {
namespace TimeUtil {
namespace {
int g_shopPeriodSeconds = kSecondsPerDay;
} // namespace

std::int64_t getMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::int64_t getSeconds() { return getMilliseconds() / kMillisecondsPerSecond; }

int getShopPeriodSeconds() { return g_shopPeriodSeconds; }
void setShopPeriodSeconds(int seconds) { g_shopPeriodSeconds = seconds; }
} // namespace TimeUtil
} // namespace server
