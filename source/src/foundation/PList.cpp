#include "foundation/PList.h"
#include "Memory.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace plist {
namespace {
std::string escape(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '&') out += "&amp;";
        else out.push_back(c);
    }
    return out;
}
std::string unescape(const std::string &s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&') {
            if (s.compare(i, 4, "&lt;") == 0) { out.push_back('<'); i += 3; continue; }
            if (s.compare(i, 4, "&gt;") == 0) { out.push_back('>'); i += 3; continue; }
            if (s.compare(i, 5, "&amp;") == 0) { out.push_back('&'); i += 4; continue; }
        }
        out.push_back(s[i]);
    }
    return out;
}
const char *kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string base64(const std::uint8_t *p, std::size_t n) {
    std::string out;
    for (std::size_t i = 0; i < n; i += 3) {
        const std::uint32_t v = (std::uint32_t(p[i]) << 16) | (i + 1 < n ? std::uint32_t(p[i + 1]) << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
        out += kB64[(v >> 18) & 63];
        out += kB64[(v >> 12) & 63];
        out += i + 1 < n ? kB64[(v >> 6) & 63] : '=';
        out += i + 2 < n ? kB64[v & 63] : '=';
    }
    return out;
}
std::string unbase64(const std::string &s) {
    std::string out;
    std::uint32_t buffer = 0;
    int bits = 0;
    for (char c : s) {
        const char *p = c ? std::strchr(kB64, c) : nullptr;
        if (!p) continue;
        buffer = (buffer << 6) | std::uint32_t(p - kB64);
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back(char((buffer >> bits) & 0xFF)); }
    }
    return out;
}
void write(ns::id o, std::string &out, int depth) {
    const std::string pad(std::size_t(depth), '\t');
    if (!o) { out += pad + "<string></string>\n"; return; }
    if (ns::isString(o)) { out += pad + "<string>" + escape(ns::text(o)) + "</string>\n"; return; }
    if (auto *n = objc::host<ns::NumberData>(o)) {
        if (n->kind == ns::NumberData::Bool) out += pad + (n->i ? "<true/>\n" : "<false/>\n");
        else if (n->kind == ns::NumberData::Double) { char b[64]; std::snprintf(b, sizeof b, "%.17g", n->d); out += pad + "<real>" + b + "</real>\n"; }
        else out += pad + "<integer>" + std::to_string(n->i) + "</integer>\n";
        return;
    }
    if (objc::host<ns::DataBytes>(o)) {
        out += pad + "<data>" + base64(mem::base() + ns::dataBytes(o), std::size_t(ns::dataLength(o))) + "</data>\n";
        return;
    }
    if (auto *d = objc::host<ns::DictData>(o)) {
        out += pad + "<dict>\n";
        for (auto &[k, v] : d->items) {
            out += pad + "\t<key>" + escape(ns::text(k)) + "</key>\n";
            write(v, out, depth + 1);
        }
        out += pad + "</dict>\n";
        return;
    }
    if (objc::host<ns::ArrayData>(o)) {
        out += pad + "<array>\n";
        for (auto i : ns::arrayItems(o)) write(i, out, depth + 1);
        out += pad + "</array>\n";
        return;
    }
    if (objc::isKindOf(o, "NSDate")) {
        char b[64];
        std::snprintf(b, sizeof b, "%.6f", cpu::d(0));
        out += pad + "<real>" + b + "</real>\n";
        return;
    }
    out += pad + "<string>" + escape(ns::describe(o)) + "</string>\n";
}
struct Parser {
    const std::string &s;
    std::size_t i = 0;
    bool mutableContainers;
    void skip() { while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i; }
    std::string tag() {
        skip();
        if (i >= s.size() || s[i] != '<') return {};
        const auto end = s.find('>', i);
        const auto t = s.substr(i + 1, end - i - 1);
        i = end + 1;
        return t;
    }
    std::string textUntil(const std::string &close) {
        const auto end = s.find(close, i);
        const auto t = s.substr(i, end - i);
        i = end + close.size();
        return t;
    }
    ns::id value() {
        for (;;) {
            const auto t = tag();
            if (t.empty()) return 0;
            if (t[0] == '?' || t[0] == '!' || t.rfind("plist", 0) == 0) continue;
            if (t == "string") return ns::string(unescape(textUntil("</string>")));
            if (t == "string/") return ns::string("");
            if (t == "integer") return ns::number(std::atoll(textUntil("</integer>").c_str()));
            if (t == "real") return ns::numberDouble(std::atof(textUntil("</real>").c_str()));
            if (t == "true/") return ns::numberBool(true);
            if (t == "false/") return ns::numberBool(false);
            if (t == "data") { const auto bytes = unbase64(textUntil("</data>")); return ns::data(bytes.data(), bytes.size(), mutableContainers); }
            if (t == "date") return ns::string(textUntil("</date>"));
            if (t == "dict/") return ns::dictionary({}, mutableContainers);
            if (t == "array/") return ns::array({}, mutableContainers);
            if (t == "dict") {
                std::vector<std::pair<ns::id, ns::id>> items;
                for (;;) {
                    skip();
                    if (s.compare(i, 7, "</dict>") == 0) { i += 7; break; }
                    if (tag() != "key") return 0;
                    const auto key = ns::string(unescape(textUntil("</key>")));
                    items.push_back({key, value()});
                }
                return ns::dictionary(items, mutableContainers);
            }
            if (t == "array") {
                std::vector<ns::id> items;
                for (;;) {
                    skip();
                    if (s.compare(i, 8, "</array>") == 0) { i += 8; break; }
                    items.push_back(value());
                }
                return ns::array(items, mutableContainers);
            }
            return 0;
        }
    }
};
} // namespace
std::string serialize(ns::id root) {
    std::string out = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
                      "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n";
    write(root, out, 0);
    return out + "</plist>\n";
}
ns::id parse(const std::string &xml, bool mutableContainers) {
    Parser p{xml, 0, mutableContainers};
    return p.value();
}
bool writeFile(ns::id root, const std::string &hostPath) {
    std::filesystem::create_directories(std::filesystem::u8path(hostPath).parent_path());
    std::ofstream out(std::filesystem::u8path(hostPath), std::ios::binary);
    const auto text = serialize(root);
    out.write(text.data(), std::streamsize(text.size()));
    return bool(out);
}
ns::id readFile(const std::string &hostPath, bool mutableContainers) {
    std::ifstream in(std::filesystem::u8path(hostPath), std::ios::binary);
    if (!in) return 0;
    const std::string text((std::istreambuf_iterator<char>(in)), {});
    if (text.rfind("bplist", 0) == 0) return 0; // binary plists are not read here
    return parse(text, mutableContainers);
}
} // namespace plist
