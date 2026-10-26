#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
// Logging in the "[tag] message" style. Channels can be silenced with HLE_QUIET=tag1,tag2.
void hlog(const char *tag, const char *format, ...);
[[noreturn]] void fatal(const char *format, ...);
void warn(const char *format, ...);
bool logEnabled(const char *tag);
