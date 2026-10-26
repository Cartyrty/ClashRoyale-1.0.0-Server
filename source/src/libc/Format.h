#pragma once
// printf-family formatting over guest arguments. Apple ARM64 passes variadic arguments on the stack in 8-byte slots
// (va_list is a plain pointer to them), so every source is "a guest pointer to consecutive 8-byte slots".
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

namespace libc {
struct ArgSource {
    std::uint64_t pointer; // next 8-byte slot
    std::uint64_t next() {
        const auto v = *reinterpret_cast<std::uint64_t *>(mem_base() + pointer);
        pointer += 8;
        return v;
    }
    double nextDouble() {
        const auto bits = next();
        double d;
        std::memcpy(&d, &bits, 8);
        return d;
    }
    static std::uint8_t *mem_base();
};
// objectFormatter handles %@ (an Objective-C object's description); nullptr prints the pointer.
std::string format(const std::string &fmt, ArgSource args, const std::function<std::string(std::uint64_t)> &objectFormatter = nullptr);
} // namespace libc
