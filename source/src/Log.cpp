#include "Log.h"
#include <mutex>
#include <set>
#include <string>

namespace {
std::mutex g_lock;
std::set<std::string> &quiet() {
    static std::set<std::string> channels = [] {
        std::set<std::string> out;
        if (const char *env = std::getenv("HLE_QUIET")) {
            std::string list = env, item;
            for (char c : list + ",") {
                if (c == ',') { if (!item.empty()) out.insert(item); item.clear(); }
                else item.push_back(c);
            }
        }
        return out;
    }();
    return channels;
}
void emit(const char *tag, const char *format, va_list args) {
    std::lock_guard lock(g_lock);
    std::fprintf(stderr, "[%s] ", tag);
    std::vfprintf(stderr, format, args);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}
} // namespace
bool logEnabled(const char *tag) { return !quiet().count(tag); }
void hlog(const char *tag, const char *format, ...) {
    if (!logEnabled(tag)) return;
    va_list args;
    va_start(args, format);
    emit(tag, format, args);
    va_end(args);
}
void warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    emit("warn", format, args);
    va_end(args);
}
void fatal(const char *format, ...) {
    va_list args;
    va_start(args, format);
    emit("fatal", format, args);
    va_end(args);
    std::exit(1);
}
