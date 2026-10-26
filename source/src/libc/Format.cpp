#include "libc/Format.h"
#include "Memory.h"
#include <cstdio>

namespace libc {
std::uint8_t *ArgSource::mem_base() { return mem::base(); }

std::string format(const std::string &fmt, ArgSource args, const std::function<std::string(std::uint64_t)> &objectFormatter) {
    std::string out;
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        const char c = fmt[i];
        if (c != '%') { out.push_back(c); continue; }
        if (i + 1 < fmt.size() && fmt[i + 1] == '%') { out.push_back('%'); ++i; continue; }
        // %[argpos$][flags][width][.precision][length]conversion
        std::string spec = "%";
        std::size_t j = i + 1;
        while (j < fmt.size() && std::strchr("-+ #0'", fmt[j])) spec.push_back(fmt[j++]);
        if (j < fmt.size() && fmt[j] == '*') { spec += std::to_string(std::int32_t(args.next())); ++j; }
        while (j < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[j]))) spec.push_back(fmt[j++]);
        if (j < fmt.size() && fmt[j] == '.') {
            spec.push_back(fmt[j++]);
            if (j < fmt.size() && fmt[j] == '*') { spec += std::to_string(std::int32_t(args.next())); ++j; }
            while (j < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[j]))) spec.push_back(fmt[j++]);
        }
        std::string length;
        while (j < fmt.size() && std::strchr("hlLqjztI", fmt[j])) length.push_back(fmt[j++]);
        if (j >= fmt.size()) break;
        const char conv = fmt[j];
        i = j;
        char buffer[512];
        switch (conv) {
        case 'd': case 'i': {
            const auto raw = args.next();
            long long v;
            if (length == "hh") v = std::int8_t(raw);
            else if (length == "h") v = std::int16_t(raw);
            else if (length.empty()) v = std::int32_t(raw);
            else v = std::int64_t(raw);
            std::snprintf(buffer, sizeof buffer, (spec + "lld").c_str(), v);
            out += buffer;
            break;
        }
        case 'u': case 'x': case 'X': case 'o': {
            const auto raw = args.next();
            unsigned long long v;
            if (length == "hh") v = std::uint8_t(raw);
            else if (length == "h") v = std::uint16_t(raw);
            else if (length.empty()) v = std::uint32_t(raw);
            else v = raw;
            std::snprintf(buffer, sizeof buffer, (spec + "ll" + conv).c_str(), v);
            out += buffer;
            break;
        }
        case 'D': case 'U': case 'O': {
            const auto raw = args.next();
            std::snprintf(buffer, sizeof buffer, (spec + "ll" + char(std::tolower(conv) == 'd' ? 'd' : std::tolower(conv))).c_str(), (long long)raw);
            out += buffer;
            break;
        }
        case 'c': case 'C': {
            const auto raw = args.next();
            out.push_back(char(raw));
            break;
        }
        case 's': {
            const auto p = args.next();
            const std::string text = p ? mem::cstring(p) : "(null)";
            if (spec == "%") out += text;
            else {
                std::string formatted(text.size() + 512, '\0');
                const int n = std::snprintf(formatted.data(), formatted.size(), (spec + "s").c_str(), text.c_str());
                formatted.resize(std::size_t(std::max(n, 0)));
                out += formatted;
            }
            break;
        }
        case 'S': { // UTF-16 string
            const auto p = args.next();
            for (std::uint64_t q = p; q && mem::read<std::uint16_t>(q); q += 2) {
                const auto u = mem::read<std::uint16_t>(q);
                if (u < 0x80) out.push_back(char(u));
                else if (u < 0x800) { out.push_back(char(0xC0 | (u >> 6))); out.push_back(char(0x80 | (u & 63))); }
                else { out.push_back(char(0xE0 | (u >> 12))); out.push_back(char(0x80 | ((u >> 6) & 63))); out.push_back(char(0x80 | (u & 63))); }
            }
            break;
        }
        case 'p': {
            std::snprintf(buffer, sizeof buffer, "0x%llx", (unsigned long long)args.next());
            out += buffer;
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            const double v = args.nextDouble();
            std::snprintf(buffer, sizeof buffer, (spec + (length == "L" ? "" : "") + conv).c_str(), v);
            out += buffer;
            break;
        }
        case '@': {
            const auto object = args.next();
            out += objectFormatter ? objectFormatter(object) : std::string("<object>");
            break;
        }
        case 'n': args.next(); break;
        default:
            out += spec + length + conv;
            break;
        }
    }
    return out;
}
} // namespace libc
