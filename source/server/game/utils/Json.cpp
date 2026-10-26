#include "game/utils/Json.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace json {
static const Value kNull;

bool Value::has(const std::string &key) const {
    for (auto &m : members_)
        if (m.first == key) return true;
    return false;
}
const Value &Value::operator[](const std::string &key) const {
    for (auto &m : members_)
        if (m.first == key) return m.second;
    return kNull;
}
Value &Value::operator[](const std::string &key) {
    type_ = Type::Object;
    for (auto &m : members_)
        if (m.first == key) return m.second;
    members_.emplace_back(key, Value());
    return members_.back().second;
}

static void dumpString(std::string &out, const std::string &s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char b[8];
                std::snprintf(b, sizeof b, "\\u%04x", c);
                out += b;
            } else
                out += char(c);
        }
    }
    out += '"';
}
static void dumpValue(std::string &out, const Value &v) {
    switch (v.type()) {
    case Value::Type::Null: out += "null"; break;
    case Value::Type::Bool: out += v.boolean() ? "true" : "false"; break;
    case Value::Type::Number: {
        const double n = v.number();
        char b[40];
        if (n == std::floor(n) && std::fabs(n) < 9.0e15)
            std::snprintf(b, sizeof b, "%lld", (long long)n);
        else
            std::snprintf(b, sizeof b, "%.17g", n);
        out += b;
        break;
    }
    case Value::Type::String: dumpString(out, v.str()); break;
    case Value::Type::Array: {
        out += '[';
        bool first = true;
        for (auto &item : v.items()) {
            if (!first) out += ',';
            first = false;
            dumpValue(out, item);
        }
        out += ']';
        break;
    }
    case Value::Type::Object: {
        out += '{';
        bool first = true;
        for (auto &m : v.members()) {
            if (!first) out += ',';
            first = false;
            dumpString(out, m.first);
            out += ':';
            dumpValue(out, m.second);
        }
        out += '}';
        break;
    }
    }
}
std::string Value::dump() const {
    std::string out;
    dumpValue(out, *this);
    return out;
}

namespace {
struct Parser {
    const std::string &s;
    std::size_t i = 0;
    bool ok = true;
    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) ++i;
    }
    bool eat(char c) {
        ws();
        if (i < s.size() && s[i] == c) {
            ++i;
            return true;
        }
        return false;
    }
    static void utf8(std::string &out, unsigned cp) {
        if (cp < 0x80)
            out += char(cp);
        else if (cp < 0x800) {
            out += char(0xC0 | (cp >> 6));
            out += char(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += char(0xE0 | (cp >> 12));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        } else {
            out += char(0xF0 | (cp >> 18));
            out += char(0x80 | ((cp >> 12) & 0x3F));
            out += char(0x80 | ((cp >> 6) & 0x3F));
            out += char(0x80 | (cp & 0x3F));
        }
    }
    std::string string() {
        std::string out;
        if (!eat('"')) {
            ok = false;
            return out;
        }
        while (i < s.size() && s[i] != '"') {
            char c = s[i++];
            if (c != '\\') {
                out += c;
                continue;
            }
            if (i >= s.size()) break;
            c = s[i++];
            switch (c) {
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u': {
                unsigned cp = unsigned(std::strtoul(s.substr(i, 4).c_str(), nullptr, 16));
                i += 4;
                if (cp >= 0xD800 && cp < 0xDC00 && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                    const unsigned low = unsigned(std::strtoul(s.substr(i + 2, 4).c_str(), nullptr, 16));
                    i += 6;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                }
                utf8(out, cp);
                break;
            }
            default: out += c;
            }
        }
        ++i;
        return out;
    }
    Value value() {
        ws();
        if (i >= s.size()) {
            ok = false;
            return {};
        }
        const char c = s[i];
        if (c == '{') {
            ++i;
            Value v = Value::object();
            if (eat('}')) return v;
            do {
                const auto key = string();
                if (!eat(':')) {
                    ok = false;
                    return v;
                }
                v[key] = value();
            } while (ok && eat(','));
            if (!eat('}')) ok = false;
            return v;
        }
        if (c == '[') {
            ++i;
            Value v = Value::array();
            if (eat(']')) return v;
            do v.push(value());
            while (ok && eat(','));
            if (!eat(']')) ok = false;
            return v;
        }
        if (c == '"') return Value(string());
        if (s.compare(i, 4, "true") == 0) {
            i += 4;
            return Value(true);
        }
        if (s.compare(i, 5, "false") == 0) {
            i += 5;
            return Value(false);
        }
        if (s.compare(i, 4, "null") == 0) {
            i += 4;
            return Value();
        }
        char *end = nullptr;
        const double n = std::strtod(s.c_str() + i, &end);
        if (end == s.c_str() + i) {
            ok = false;
            return {};
        }
        i = std::size_t(end - s.c_str());
        return Value(n);
    }
};
} // namespace
Value Value::parse(const std::string &text, bool *ok) {
    Parser p{text};
    auto v = p.value();
    if (ok) *ok = p.ok;
    return v;
}
} // namespace json
