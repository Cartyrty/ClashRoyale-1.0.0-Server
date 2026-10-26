#include "titan/logic/debug/Debugger.h"

#include "Log.h"

#include <cstdarg>
#include <cstdio>
#include <string>

namespace server {
namespace {
std::string formatMessage(const char *format, va_list arguments) {
    va_list copy;
    va_copy(copy, arguments);
    const int length = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    if (length <= 0) return {};
    std::string text(std::size_t(length) + 1, '\0');
    std::vsnprintf(text.data(), text.size(), format, arguments);
    text.resize(std::size_t(length));
    return text;
}
} // namespace

void Debugger::print(const char *channel, const char *format, ...) {
    if (!logEnabled(channel)) return;
    va_list arguments;
    va_start(arguments, format);
    const auto text = formatMessage(format, arguments);
    va_end(arguments);
    hlog(channel, "%s", text.c_str());
}

void Debugger::warning(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const auto text = formatMessage(format, arguments);
    va_end(arguments);
    warn("%s", text.c_str());
}

void Debugger::error(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const auto text = formatMessage(format, arguments);
    va_end(arguments);
    fatal("%s", text.c_str());
}

void Debugger::doAssert(bool condition, const char *message) {
    if (!condition) error("assertion failed: %s", message);
}
} // namespace server
