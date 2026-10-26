// NSString / NSMutableString / constant strings (__cfstring) and the CFString functions.
#include "foundation/Foundation.h"
#include "Loader.h"
#include "Log.h"
#include "Memory.h"
#include "libc/Files.h"
#include "libc/Format.h"
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>

namespace ns {
using objc::marg;
using objc::self;
using hle::ret;

std::u16string fromUtf8(const std::string &s) {
    std::u16string out;
    for (std::size_t i = 0; i < s.size();) {
        const auto c = static_cast<unsigned char>(s[i]);
        std::uint32_t cp;
        int extra;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { cp = 0xFFFD; extra = 0; }
        ++i;
        for (int k = 0; k < extra && i < s.size(); ++k, ++i) cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
        if (cp >= 0x10000) {
            cp -= 0x10000;
            out.push_back(char16_t(0xD800 + (cp >> 10)));
            out.push_back(char16_t(0xDC00 + (cp & 0x3FF)));
        } else out.push_back(char16_t(cp));
    }
    return out;
}
std::string toUtf8(const std::u16string &s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        std::uint32_t cp = s[i];
        if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < s.size()) cp = 0x10000 + ((cp - 0xD800) << 10) + (s[++i] - 0xDC00);
        if (cp < 0x80) out.push_back(char(cp));
        else if (cp < 0x800) { out.push_back(char(0xC0 | (cp >> 6))); out.push_back(char(0x80 | (cp & 63))); }
        else if (cp < 0x10000) {
            out.push_back(char(0xE0 | (cp >> 12)));
            out.push_back(char(0x80 | ((cp >> 6) & 63)));
            out.push_back(char(0x80 | (cp & 63)));
        } else {
            out.push_back(char(0xF0 | (cp >> 18)));
            out.push_back(char(0x80 | ((cp >> 12) & 63)));
            out.push_back(char(0x80 | ((cp >> 6) & 63)));
            out.push_back(char(0x80 | (cp & 63)));
        }
    }
    return out;
}

namespace {
objc::Class g_stringClass = 0, g_constantClass = 0;
objc::Class stringClass() {
    if (!g_stringClass) g_stringClass = objc::getClass("__NSCFString");
    return g_stringClass;
}
objc::Class constantClass() {
    if (!g_constantClass) g_constantClass = objc::getClass("__NSCFConstantString");
    return g_constantClass;
}
// Constant strings ({isa, flags, chars, length}): contents are built on first use and cached.
StringData *data(id s) {
    if (!s) return nullptr;
    if (auto *d = objc::host<StringData>(s)) return d;
    if (objc::classOf(s) == constantClass()) {
        auto d = std::make_unique<StringData>();
        const auto flags = mem::r64(s + 8), chars = mem::r64(s + 16), length = mem::r64(s + 24);
        if ((flags & 0x10) == 0x10 && (flags & 0xFF) == 0xD0) { // UTF-16 constant (0x7D0)
            for (std::uint64_t i = 0; i < length; ++i) d->text.push_back(mem::read<char16_t>(chars + 2 * i));
        } else {
            d->text = fromUtf8(std::string(reinterpret_cast<char *>(mem::base() + chars), std::size_t(length)));
        }
        auto *raw = d.get();
        objc::setHostObject(s, std::move(d));
        return raw;
    }
    return nullptr;
}
std::u16string arg16(int i) { return text16(marg(i)); }
id makeOn(id object, std::u16string text, bool mutableString) {
    auto d = std::make_unique<StringData>();
    d->text = std::move(text);
    d->mutableString = mutableString;
    objc::setHostObject(object, std::move(d));
    return object;
}
std::u16string lower(std::u16string s) {
    for (auto &c : s) if (c < 0x80) c = char16_t(std::tolower(c)); else c = char16_t(std::towlower(std::wint_t(c)));
    return s;
}
std::u16string upper(std::u16string s) {
    for (auto &c : s) if (c < 0x80) c = char16_t(std::toupper(c)); else c = char16_t(std::towupper(std::wint_t(c)));
    return s;
}
// NSString compare:options: (NSCaseInsensitiveSearch 1, NSLiteralSearch 2, NSNumericSearch 64).
int compareStrings(std::u16string a, std::u16string b, std::uint64_t options) {
    if (options & 1) { a = lower(a); b = lower(b); }
    if (options & 64) {
        std::size_t i = 0, j = 0;
        while (i < a.size() && j < b.size()) {
            if (std::iswdigit(a[i]) && std::iswdigit(b[j])) {
                std::uint64_t x = 0, y = 0;
                while (i < a.size() && std::iswdigit(a[i])) x = x * 10 + (a[i++] - u'0');
                while (j < b.size() && std::iswdigit(b[j])) y = y * 10 + (b[j++] - u'0');
                if (x != y) return x < y ? -1 : 1;
            } else {
                if (a[i] != b[j]) return a[i] < b[j] ? -1 : 1;
                ++i; ++j;
            }
        }
        return i < a.size() ? 1 : j < b.size() ? -1 : 0;
    }
    const int r = a.compare(b);
    return r < 0 ? -1 : r > 0 ? 1 : 0;
}
std::string fmtArgs(std::uint64_t format, std::uint64_t argPointer) {
    return libc::format(text(format), {argPointer}, [](std::uint64_t object) { return object ? describe(object) : std::string("(null)"); });
}
void retRange(std::uint64_t location, std::uint64_t length) {
    cpu::setX(0, location);
    cpu::setX(1, length);
}
constexpr std::uint64_t kNotFound = 0x7FFFFFFFFFFFFFFFull;
} // namespace

id stringU16(const std::u16string &utf16, bool mutableString) {
    return makeOn(objc::allocInstance(stringClass()), utf16, mutableString);
}
id string(const std::string &utf8) { return stringU16(fromUtf8(utf8)); }
bool isString(id object) { return object && (data(object) != nullptr || objc::isKindOf(object, "NSString")); }
std::u16string text16(id s) {
    if (auto *d = data(s)) return d->text;
    if (s && objc::isKindOf(s, "NSString")) {
        // A guest NSString subclass: ask it.
        const auto n = objc::send(s, "length");
        std::u16string out;
        for (std::uint64_t i = 0; i < n; ++i) out.push_back(char16_t(objc::send(s, "characterAtIndex:", {i})));
        return out;
    }
    return {};
}
std::string text(id s) { return toUtf8(text16(s)); }
std::uint64_t utf8Pointer(id s) {
    auto *d = data(s);
    if (!d) return s ? mem::heapString(text(s)) : 0;
    const auto utf8 = toUtf8(d->text);
    if (d->utf8Buffer && mem::cstring(d->utf8Buffer) == utf8) return d->utf8Buffer;
    d->utf8Buffer = mem::heapString(utf8);
    return d->utf8Buffer;
}
std::string describe(id object) {
    if (!object) return "(null)";
    if (isString(object)) return text(object);
    return text(objc::send(object, "description"));
}
std::string className(id object) { return objc::className(objc::classOf(object)); }

// The class constant strings point at.
static hle::DataRegistrar d_cfConstant("___CFConstantStringClassReference", [] { return constantClass(); });
// Framework NSString constants (NSFontAttributeName, UIApplicationLaunchOptionsURLKey...): a constant string whose
// contents are the symbol name, which is what the real values are in practice.
static const bool g_stringConstants = [] {
    for (const char *prefix : {"_NS", "_UI", "_kCF", "_kCT", "_kSec", "_kEAGL", "_kUT", "_MP", "_GK", "_SK", "_AD", "_CTRadio"}) {
        hle::registerDataPrefix(prefix, [](const std::string &name) -> std::uint64_t {
            if (!hle::looksLikeData(name)) return 0;
            const auto pointer = mem::hleAlloc(8);
            const auto str = mem::hleAlloc(32, 8);
            mem::w64(str, constantClass());
            mem::w64(str + 8, 0x7C8);
            const auto bare = name.substr(1);
            mem::w64(str + 16, mem::hleString(bare));
            mem::w64(str + 24, bare.size());
            mem::w64(pointer, str);
            return pointer;
        });
    }
    return true;
}();
} // namespace ns

using namespace ns;
using objc::marg;
using objc::self;
using hle::ret;

OBJC_CLASS(NSString, NSObject);
OBJC_CLASS(NSMutableString, NSString);
OBJC_CLASS(__NSCFString, NSMutableString);
OBJC_CLASS(__NSCFConstantString, __NSCFString);

// Allocation: every string instance is an __NSCFString with host contents.
OBJC_CLASS_METHOD(NSString, "alloc", alloc) { ret(makeOn(objc::allocInstance(objc::getClass("__NSCFString")), u"", false)); }
OBJC_CLASS_METHOD(NSString, "allocWithZone:", allocWithZone) { ret(makeOn(objc::allocInstance(objc::getClass("__NSCFString")), u"", false)); }
OBJC_CLASS_METHOD(NSMutableString, "alloc", alloc) { ret(makeOn(objc::allocInstance(objc::getClass("__NSCFString")), u"", true)); }
OBJC_CLASS_METHOD(NSString, "string", string_) { ret(stringU16(u"")); }
OBJC_CLASS_METHOD(NSMutableString, "string", string_) { ret(stringU16(u"", true)); }
OBJC_CLASS_METHOD(NSMutableString, "stringWithCapacity:", withCapacity) { ret(stringU16(u"", true)); }
OBJC_CLASS_METHOD(NSString, "stringWithUTF8String:", withUTF8) { ret(marg(0) ? string(mem::cstring(marg(0))) : 0); }
OBJC_CLASS_METHOD(NSString, "stringWithCString:", withCString) { ret(marg(0) ? string(mem::cstring(marg(0))) : 0); }
OBJC_CLASS_METHOD(NSString, "stringWithCString:encoding:", withCStringEnc) { ret(marg(0) ? string(mem::cstring(marg(0))) : 0); }
OBJC_CLASS_METHOD(NSString, "stringWithString:", withString) {
    const bool m = objc::isSubclass(self(), objc::getClass("NSMutableString"));
    ret(stringU16(text16(marg(0)), m));
}
OBJC_CLASS_METHOD(NSString, "stringWithFormat:", withFormat) {
    const bool m = objc::isSubclass(self(), objc::getClass("NSMutableString"));
    ret(stringU16(fromUtf8(fmtArgs(marg(0), cpu::sp())), m));
}
OBJC_CLASS_METHOD(NSString, "stringWithCharacters:length:", withCharacters) {
    std::u16string s;
    for (std::uint64_t i = 0; i < marg(1); ++i) s.push_back(mem::read<char16_t>(marg(0) + 2 * i));
    ret(stringU16(s));
}
OBJC_CLASS_METHOD(NSString, "stringWithContentsOfFile:encoding:error:", withContentsOfFile) {
    std::string bytes;
    if (!files::readWholeFile(text(marg(0)), bytes)) return ret(0);
    ret(string(bytes));
}
OBJC_CLASS_METHOD(NSString, "stringWithContentsOfFile:", withContentsOfFile1) {
    std::string bytes;
    if (!files::readWholeFile(text(marg(0)), bytes)) return ret(0);
    ret(string(bytes));
}
OBJC_CLASS_METHOD(NSString, "defaultCStringEncoding", defaultEncoding) { ret(30); }
OBJC_CLASS_METHOD(NSString, "pathWithComponents:", pathWithComponents) {
    std::u16string out;
    for (const auto item : arrayItems(marg(0))) {
        const auto part = text16(item);
        if (!out.empty() && out.back() != u'/') out += u'/';
        out += part;
    }
    ret(stringU16(out));
}

// Initializers (self was allocated by +alloc above, or is a guest subclass instance).
OBJC_METHOD(NSString, "init", init) { ret(self()); }
OBJC_METHOD(NSString, "initWithUTF8String:", initWithUTF8) {
    if (!marg(0)) return ret(0);
    auto *d = data(self());
    if (d) d->text = fromUtf8(mem::cstring(marg(0)));
    ret(self());
}
OBJC_METHOD(NSString, "initWithCString:encoding:", initWithCString) {
    if (!marg(0)) return ret(0);
    if (auto *d = data(self())) d->text = fromUtf8(mem::cstring(marg(0)));
    ret(self());
}
OBJC_METHOD(NSString, "initWithString:", initWithString) {
    if (auto *d = data(self())) d->text = text16(marg(0));
    ret(self());
}
OBJC_METHOD(NSString, "initWithFormat:", initWithFormat) {
    if (auto *d = data(self())) d->text = fromUtf8(fmtArgs(marg(0), cpu::sp()));
    ret(self());
}
OBJC_METHOD(NSString, "initWithFormat:arguments:", initWithFormatArgs) {
    if (auto *d = data(self())) d->text = fromUtf8(fmtArgs(marg(0), marg(1)));
    ret(self());
}
OBJC_METHOD(NSString, "initWithFormat:locale:arguments:", initWithFormatLocaleArgs) {
    if (auto *d = data(self())) d->text = fromUtf8(fmtArgs(marg(0), marg(2)));
    ret(self());
}
static std::u16string decodeBytes(std::uint64_t bytes, std::uint64_t length, std::uint64_t encoding) {
    if (encoding == 10 || encoding == 0x94000100 || encoding == 0x90000100) {
        std::u16string s;
        for (std::uint64_t i = 0; i + 1 < length + 1 && i < length / 2; ++i) s.push_back(mem::read<char16_t>(bytes + 2 * i));
        return s;
    }
    const std::string raw(reinterpret_cast<char *>(mem::base() + bytes), std::size_t(length));
    if (encoding == 5 || encoding == 12 || encoding == 30) {
        std::u16string s;
        for (unsigned char c : raw) s.push_back(c);
        return s;
    }
    return fromUtf8(raw);
}
OBJC_METHOD(NSString, "initWithBytes:length:encoding:", initWithBytes) {
    if (auto *d = data(self())) d->text = decodeBytes(marg(0), marg(1), marg(2));
    ret(self());
}
OBJC_METHOD(NSString, "initWithBytesNoCopy:length:encoding:freeWhenDone:", initWithBytesNoCopy) {
    if (auto *d = data(self())) d->text = decodeBytes(marg(0), marg(1), marg(2));
    ret(self());
}
OBJC_METHOD(NSString, "initWithData:encoding:", initWithData) {
    if (!marg(0)) return ret(0);
    if (auto *d = data(self())) d->text = decodeBytes(dataBytes(marg(0)), dataLength(marg(0)), marg(1));
    ret(self());
}
OBJC_METHOD(NSString, "initWithCharacters:length:", initWithCharacters) {
    std::u16string s;
    for (std::uint64_t i = 0; i < marg(1); ++i) s.push_back(mem::read<char16_t>(marg(0) + 2 * i));
    if (auto *d = data(self())) d->text = s;
    ret(self());
}
OBJC_METHOD(NSString, "initWithContentsOfFile:encoding:error:", initWithContentsOfFile) {
    std::string bytes;
    if (!files::readWholeFile(text(marg(0)), bytes)) return ret(0);
    if (auto *d = data(self())) d->text = fromUtf8(bytes);
    ret(self());
}

// Accessors.
OBJC_METHOD(NSString, "length", length) { ret(text16(self()).size()); }
OBJC_METHOD(NSString, "characterAtIndex:", characterAtIndex) {
    const auto t = text16(self());
    ret(marg(0) < t.size() ? t[std::size_t(marg(0))] : 0);
}
OBJC_METHOD(NSString, "getCharacters:range:", getCharactersRange) {
    const auto t = text16(self());
    for (std::uint64_t i = 0; i < marg(2) && marg(1) + i < t.size(); ++i) mem::write<char16_t>(marg(0) + 2 * i, t[std::size_t(marg(1) + i)]);
}
OBJC_METHOD(NSString, "getCharacters:", getCharacters) {
    const auto t = text16(self());
    for (std::size_t i = 0; i < t.size(); ++i) mem::write<char16_t>(marg(0) + 2 * i, t[i]);
}
OBJC_METHOD(NSString, "UTF8String", UTF8String) { ret(utf8Pointer(self())); }
OBJC_METHOD(NSString, "cString", cString) { ret(utf8Pointer(self())); }
OBJC_METHOD(NSString, "cStringUsingEncoding:", cStringUsingEncoding) { ret(utf8Pointer(self())); }
OBJC_METHOD(NSString, "fileSystemRepresentation", fileSystemRepresentation) { ret(utf8Pointer(self())); }
OBJC_METHOD(NSString, "getCString:maxLength:encoding:", getCString) {
    const auto s = text(self());
    if (s.size() + 1 > marg(1)) return ret(0);
    std::memcpy(mem::base() + marg(0), s.c_str(), s.size() + 1);
    ret(1);
}
OBJC_METHOD(NSString, "getFileSystemRepresentation:maxLength:", getFileSystemRepresentation) {
    const auto s = text(self());
    if (s.size() + 1 > marg(1)) return ret(0);
    std::memcpy(mem::base() + marg(0), s.c_str(), s.size() + 1);
    ret(1);
}
OBJC_METHOD(NSString, "lengthOfBytesUsingEncoding:", lengthOfBytes) {
    ret(marg(0) == 10 ? text16(self()).size() * 2 : text(self()).size());
}
OBJC_METHOD(NSString, "maximumLengthOfBytesUsingEncoding:", maxLengthOfBytes) { ret(text16(self()).size() * 4); }
OBJC_METHOD(NSString, "dataUsingEncoding:", dataUsingEncoding) {
    const auto s = text(self());
    ret(ns::data(s.data(), s.size()));
}
OBJC_METHOD(NSString, "dataUsingEncoding:allowLossyConversion:", dataUsingEncodingLossy) {
    const auto s = text(self());
    ret(ns::data(s.data(), s.size()));
}
OBJC_METHOD(NSString, "description", description) { ret(self()); }
OBJC_METHOD(NSString, "copyWithZone:", copyWithZone) { ret(stringU16(text16(self()))); }
OBJC_METHOD(NSString, "mutableCopyWithZone:", mutableCopyWithZone) { ret(stringU16(text16(self()), true)); }
OBJC_METHOD(NSString, "hash", hash) {
    std::uint64_t h = 1469598103934665603ull;
    for (auto c : text16(self())) h = (h ^ c) * 1099511628211ull;
    ret(h & 0x7FFFFFFFFFFFFFFFull);
}
OBJC_METHOD(NSString, "isEqual:", isEqual) { ret(marg(0) && isString(marg(0)) && text16(self()) == text16(marg(0))); }
OBJC_METHOD(NSString, "isEqualToString:", isEqualToString) { ret(marg(0) && text16(self()) == text16(marg(0))); }
OBJC_METHOD(NSString, "compare:", compare) { ret(std::uint64_t(std::int64_t(compareStrings(text16(self()), text16(marg(0)), 0)))); }
OBJC_METHOD(NSString, "compare:options:", compareOptions) {
    ret(std::uint64_t(std::int64_t(compareStrings(text16(self()), text16(marg(0)), marg(1)))));
}
OBJC_METHOD(NSString, "compare:options:range:", compareOptionsRange) {
    const auto t = text16(self()).substr(std::size_t(marg(2)), std::size_t(marg(3)));
    ret(std::uint64_t(std::int64_t(compareStrings(t, text16(marg(0)), marg(1)))));
}
OBJC_METHOD(NSString, "caseInsensitiveCompare:", caseInsensitiveCompare) {
    ret(std::uint64_t(std::int64_t(compareStrings(text16(self()), text16(marg(0)), 1))));
}
OBJC_METHOD(NSString, "localizedCaseInsensitiveCompare:", localizedCaseInsensitiveCompare) {
    ret(std::uint64_t(std::int64_t(compareStrings(text16(self()), text16(marg(0)), 1))));
}
OBJC_METHOD(NSString, "hasPrefix:", hasPrefix) {
    const auto t = text16(self()), p = text16(marg(0));
    ret(!p.empty() && t.compare(0, p.size(), p) == 0);
}
OBJC_METHOD(NSString, "hasSuffix:", hasSuffix) {
    const auto t = text16(self()), p = text16(marg(0));
    ret(!p.empty() && t.size() >= p.size() && t.compare(t.size() - p.size(), p.size(), p) == 0);
}
// rangeOfComposedCharacterSequencesForRange:(NSRange) - widened so it never splits a surrogate pair (UIKit's
// deleteBackward uses it to remove the last character).
OBJC_METHOD(NSString, "rangeOfComposedCharacterSequencesForRange:", rangeOfComposedCharacterSequencesForRange) {
    const auto t = text16(self());
    std::uint64_t start = std::min<std::uint64_t>(marg(0), t.size()), end = std::min<std::uint64_t>(marg(0) + marg(1), t.size());
    if (start > 0 && start < t.size() && t[start] >= 0xDC00 && t[start] <= 0xDFFF) --start;
    if (end > 0 && end < t.size() && t[end] >= 0xDC00 && t[end] <= 0xDFFF) ++end;
    retRange(start, end - start);
}
OBJC_METHOD(NSString, "rangeOfString:", rangeOfString) {
    const auto pos = text16(self()).find(text16(marg(0)));
    if (pos == std::u16string::npos || text16(marg(0)).empty()) retRange(kNotFound, 0);
    else retRange(pos, text16(marg(0)).size());
}
OBJC_METHOD(NSString, "rangeOfString:options:", rangeOfStringOptions) {
    auto t = text16(self()), n = text16(marg(0));
    if (marg(1) & 1) { t = lower(t); n = lower(n); }
    const auto pos = (marg(1) & 4) ? t.rfind(n) : t.find(n);
    if (pos == std::u16string::npos || n.empty()) retRange(kNotFound, 0);
    else retRange(pos, n.size());
}
OBJC_METHOD(NSString, "containsString:", containsString) { ret(text16(self()).find(text16(marg(0))) != std::u16string::npos); }
OBJC_METHOD(NSString, "substringFromIndex:", substringFromIndex) {
    const auto t = text16(self());
    ret(stringU16(marg(0) <= t.size() ? t.substr(std::size_t(marg(0))) : u""));
}
OBJC_METHOD(NSString, "substringToIndex:", substringToIndex) {
    const auto t = text16(self());
    ret(stringU16(t.substr(0, std::size_t(std::min<std::uint64_t>(marg(0), t.size())))));
}
OBJC_METHOD(NSString, "substringWithRange:", substringWithRange) {
    const auto t = text16(self());
    ret(stringU16(marg(0) <= t.size() ? t.substr(std::size_t(marg(0)), std::size_t(marg(1))) : u""));
}
OBJC_METHOD(NSString, "stringByAppendingString:", stringByAppendingString) { ret(stringU16(text16(self()) + text16(marg(0)))); }
OBJC_METHOD(NSString, "stringByAppendingFormat:", stringByAppendingFormat) {
    ret(stringU16(text16(self()) + fromUtf8(fmtArgs(marg(0), cpu::sp()))));
}
OBJC_METHOD(NSString, "stringByReplacingOccurrencesOfString:withString:", stringByReplacing) {
    auto t = text16(self());
    const auto from = text16(marg(0)), to = text16(marg(1));
    if (!from.empty())
        for (std::size_t pos = 0; (pos = t.find(from, pos)) != std::u16string::npos; pos += to.size()) t.replace(pos, from.size(), to);
    ret(stringU16(t));
}
OBJC_METHOD(NSString, "stringByReplacingOccurrencesOfString:withString:options:range:", stringByReplacingOptions) {
    auto t = text16(self());
    const auto from = text16(marg(0)), to = text16(marg(1));
    if (!from.empty())
        for (std::size_t pos = 0; (pos = t.find(from, pos)) != std::u16string::npos; pos += to.size()) t.replace(pos, from.size(), to);
    ret(stringU16(t));
}
OBJC_METHOD(NSString, "componentsSeparatedByString:", componentsSeparatedByString) {
    const auto t = text16(self()), sep = text16(marg(0));
    std::vector<id> parts;
    std::size_t start = 0;
    if (!sep.empty())
        for (std::size_t pos; (pos = t.find(sep, start)) != std::u16string::npos; start = pos + sep.size()) parts.push_back(stringU16(t.substr(start, pos - start)));
    parts.push_back(stringU16(t.substr(start)));
    ret(array(parts));
}
OBJC_METHOD(NSString, "stringByTrimmingCharactersInSet:", stringByTrimming) {
    auto t = text16(self());
    auto space = [](char16_t c) { return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r'; };
    while (!t.empty() && space(t.back())) t.pop_back();
    std::size_t i = 0;
    while (i < t.size() && space(t[i])) ++i;
    ret(stringU16(t.substr(i)));
}
OBJC_METHOD(NSString, "lowercaseString", lowercaseString) { ret(stringU16(lower(text16(self())))); }
OBJC_METHOD(NSString, "uppercaseString", uppercaseString) { ret(stringU16(upper(text16(self())))); }
OBJC_METHOD(NSString, "capitalizedString", capitalizedString) {
    auto t = lower(text16(self()));
    bool start = true;
    for (auto &c : t) { if (start && c < 0x80) c = char16_t(std::toupper(c)); start = c == u' '; }
    ret(stringU16(t));
}
OBJC_METHOD(NSString, "intValue", intValue_) { ret(std::uint64_t(std::int64_t(std::int32_t(std::atoll(text(self()).c_str()))))); }
OBJC_METHOD(NSString, "integerValue", integerValue) { ret(std::uint64_t(std::atoll(text(self()).c_str()))); }
OBJC_METHOD(NSString, "longLongValue", longLongValue) { ret(std::uint64_t(std::atoll(text(self()).c_str()))); }
OBJC_METHOD(NSString, "unsignedLongLongValue", unsignedLongLongValue) { ret(std::strtoull(text(self()).c_str(), nullptr, 10)); }
OBJC_METHOD(NSString, "floatValue", floatValue) { hle::retFloat(float(std::atof(text(self()).c_str()))); }
OBJC_METHOD(NSString, "doubleValue", doubleValue_) { hle::retDouble(std::atof(text(self()).c_str())); }
OBJC_METHOD(NSString, "boolValue", boolValue) {
    const auto t = lower(text16(self()));
    ret(!t.empty() && (t[0] == u'y' || t[0] == u't' || (t[0] >= u'1' && t[0] <= u'9')));
}
// Paths.
OBJC_METHOD(NSString, "lastPathComponent", lastPathComponent) {
    auto t = text16(self());
    while (t.size() > 1 && t.back() == u'/') t.pop_back();
    const auto slash = t.rfind(u'/');
    ret(stringU16(slash == std::u16string::npos || t.size() == 1 ? t : t.substr(slash + 1)));
}
OBJC_METHOD(NSString, "stringByDeletingLastPathComponent", stringByDeletingLastPathComponent) {
    auto t = text16(self());
    while (t.size() > 1 && t.back() == u'/') t.pop_back();
    const auto slash = t.rfind(u'/');
    if (slash == std::u16string::npos) return ret(stringU16(u""));
    ret(stringU16(slash == 0 ? u"/" : t.substr(0, slash)));
}
OBJC_METHOD(NSString, "stringByAppendingPathComponent:", stringByAppendingPathComponent) {
    auto t = text16(self());
    auto c = text16(marg(0));
    if (!t.empty() && t.back() != u'/' && !c.empty()) t += u'/';
    if (!c.empty() && c[0] == u'/' && !t.empty() && t.back() == u'/') c.erase(0, 1);
    ret(stringU16(t + c));
}
OBJC_METHOD(NSString, "pathExtension", pathExtension) {
    const auto t = text16(self());
    const auto dot = t.rfind(u'.'), slash = t.rfind(u'/');
    ret(stringU16(dot == std::u16string::npos || (slash != std::u16string::npos && dot < slash) ? u"" : t.substr(dot + 1)));
}
OBJC_METHOD(NSString, "stringByDeletingPathExtension", stringByDeletingPathExtension) {
    const auto t = text16(self());
    const auto dot = t.rfind(u'.'), slash = t.rfind(u'/');
    ret(stringU16(dot == std::u16string::npos || (slash != std::u16string::npos && dot < slash) ? t : t.substr(0, dot)));
}
OBJC_METHOD(NSString, "stringByAppendingPathExtension:", stringByAppendingPathExtension) {
    ret(stringU16(text16(self()) + u"." + text16(marg(0))));
}
OBJC_METHOD(NSString, "pathComponents", pathComponents) {
    const auto t = text16(self());
    std::vector<id> parts;
    if (!t.empty() && t[0] == u'/') parts.push_back(stringU16(u"/"));
    std::size_t start = 0;
    while (start <= t.size()) {
        const auto pos = t.find(u'/', start);
        const auto part = t.substr(start, pos == std::u16string::npos ? std::u16string::npos : pos - start);
        if (!part.empty()) parts.push_back(stringU16(part));
        if (pos == std::u16string::npos) break;
        start = pos + 1;
    }
    ret(array(parts));
}
OBJC_METHOD(NSString, "isAbsolutePath", isAbsolutePath) { const auto t = text16(self()); ret(!t.empty() && t[0] == u'/'); }
OBJC_METHOD(NSString, "stringByExpandingTildeInPath", stringByExpandingTilde) {
    auto t = text16(self());
    if (!t.empty() && t[0] == u'~') t = fromUtf8(files::kHomePath) + t.substr(1);
    ret(stringU16(t));
}
OBJC_METHOD(NSString, "stringByStandardizingPath", stringByStandardizingPath) { ret(self()); }
OBJC_METHOD(NSString, "stringByAddingPercentEscapesUsingEncoding:", stringByAddingPercentEscapes) {
    std::string out;
    for (unsigned char c : text(self())) {
        if (std::isalnum(c) || std::strchr("-_.~/:?=&", c)) out.push_back(char(c));
        else { char b[4]; std::snprintf(b, sizeof b, "%%%02X", c); out += b; }
    }
    ret(string(out));
}
OBJC_METHOD(NSString, "stringByAddingPercentEncodingWithAllowedCharacters:", stringByAddingPercentEncoding) {
    std::string out;
    for (unsigned char c : text(self())) {
        if (std::isalnum(c) || std::strchr("-_.~", c)) out.push_back(char(c));
        else { char b[4]; std::snprintf(b, sizeof b, "%%%02X", c); out += b; }
    }
    ret(string(out));
}
OBJC_METHOD(NSString, "writeToFile:atomically:encoding:error:", writeToFile) {
    const auto host = files::hostPath(text(marg(0)));
    std::filesystem::create_directories(std::filesystem::u8path(host).parent_path());
    std::ofstream out(std::filesystem::u8path(host), std::ios::binary);
    const auto s = text(self());
    out.write(s.data(), std::streamsize(s.size()));
    ret(bool(out));
}
OBJC_METHOD(NSString, "sizeWithFont:", sizeWithFont) { cpu::setD(0, 10.0 * double(text16(self()).size())); cpu::setD(1, 20); }

// NSMutableString.
static StringData *mutableData(id s) {
    auto *d = data(s);
    return d;
}
OBJC_METHOD(NSMutableString, "appendString:", appendString) { if (auto *d = mutableData(self())) d->text += text16(marg(0)); }
OBJC_METHOD(NSMutableString, "appendFormat:", appendFormat) {
    if (auto *d = mutableData(self())) d->text += fromUtf8(fmtArgs(marg(0), cpu::sp()));
}
OBJC_METHOD(NSMutableString, "setString:", setString) { if (auto *d = mutableData(self())) d->text = text16(marg(0)); }
OBJC_METHOD(NSMutableString, "deleteCharactersInRange:", deleteCharactersInRange) {
    if (auto *d = mutableData(self())) if (marg(0) <= d->text.size()) d->text.erase(std::size_t(marg(0)), std::size_t(marg(1)));
}
OBJC_METHOD(NSMutableString, "insertString:atIndex:", insertString) {
    if (auto *d = mutableData(self())) if (marg(1) <= d->text.size()) d->text.insert(std::size_t(marg(1)), text16(marg(0)));
}
OBJC_METHOD(NSMutableString, "replaceCharactersInRange:withString:", replaceCharacters) {
    if (auto *d = mutableData(self())) if (marg(0) <= d->text.size()) d->text.replace(std::size_t(marg(0)), std::size_t(marg(1)), text16(marg(2)));
}
OBJC_METHOD(NSMutableString, "replaceOccurrencesOfString:withString:options:range:", replaceOccurrences) {
    auto *d = mutableData(self());
    if (!d) return ret(0);
    const auto from = text16(marg(0)), to = text16(marg(1));
    std::uint64_t count = 0;
    if (!from.empty())
        for (std::size_t pos = 0; (pos = d->text.find(from, pos)) != std::u16string::npos; pos += to.size(), ++count) d->text.replace(pos, from.size(), to);
    ret(count);
}

// --- CFString ---
using hle::arg;
HLE(CFStringCreateWithCString) { ret(arg(1) ? string(mem::cstring(arg(1))) : 0); }
HLE(CFStringCreateWithCStringNoCopy) { ret(arg(1) ? string(mem::cstring(arg(1))) : 0); }
HLE(CFStringCreateWithBytes) { ret(stringU16(decodeBytes(arg(1), arg(2), arg(3) == 0x08000100 ? 4 : arg(3) == 0x0600 ? 1 : 4))); }
HLE(CFStringCreateWithSubstring) {
    const auto t = text16(arg(1));
    ret(stringU16(arg(2) <= t.size() ? t.substr(std::size_t(arg(2)), std::size_t(arg(3))) : u""));
}
HLE(CFStringCreateCopy) { ret(stringU16(text16(arg(1)))); }
HLE(CFStringCreateMutable) { ret(stringU16(u"", true)); }
HLE(CFStringCreateMutableCopy) { ret(stringU16(text16(arg(2)), true)); }
HLE(CFStringGetLength) { ret(text16(arg(0)).size()); }
HLE(CFStringGetCharacterAtIndex) {
    const auto t = text16(arg(0));
    ret(arg(1) < t.size() ? t[std::size_t(arg(1))] : 0);
}
HLE(CFStringGetCStringPtr) { ret(utf8Pointer(arg(0))); }
HLE(CFStringGetCharactersPtr) { ret(0); }
HLE(CFStringGetCString) {
    const auto s = text(arg(0));
    if (s.size() + 1 > arg(2)) return ret(0);
    std::memcpy(mem::base() + arg(1), s.c_str(), s.size() + 1);
    ret(1);
}
HLE(CFStringGetCharacters) {
    const auto t = text16(arg(0));
    for (std::uint64_t i = 0; i < arg(2) && arg(1) + i < t.size(); ++i) mem::write<char16_t>(arg(3) + 2 * i, t[std::size_t(arg(1) + i)]);
}
// CFStringGetBytes(str, range{loc,len}, encoding, lossByte, isExternal, buffer, maxBufLen, usedBufLen*) -> chars converted
HLE(CFStringGetBytes) {
    const auto t = text16(arg(0)).substr(std::size_t(arg(1)), std::size_t(arg(2)));
    const auto encoding = std::uint32_t(arg(3));
    const auto buffer = arg(6), max = arg(7), used = mem::r64(cpu::sp());
    std::string bytes;
    if (encoding == 0x100 || encoding == 0x14000100) for (auto c : t) { bytes.push_back(char(c & 0xFF)); bytes.push_back(char(c >> 8)); }
    else bytes = toUtf8(t);
    const auto n = buffer ? std::min<std::uint64_t>(bytes.size(), max) : bytes.size();
    if (buffer) std::memcpy(mem::base() + buffer, bytes.data(), std::size_t(n));
    if (used) mem::w64(used, n);
    ret(t.size());
}
HLE(CFStringAppendCharacters) {
    if (auto *d = data(arg(0)))
        for (std::uint64_t i = 0; i < arg(2); ++i) d->text.push_back(mem::read<char16_t>(arg(1) + 2 * i));
}
HLE(CFStringAppend) { if (auto *d = data(arg(0))) d->text += text16(arg(1)); }
HLE(CFStringAppendCString) { if (auto *d = data(arg(0))) d->text += fromUtf8(mem::cstring(arg(1))); }
HLE(CFStringConvertNSStringEncodingToEncoding) {
    switch (arg(0)) {
    case 4: ret(0x08000100); break;
    case 1: ret(0x0600); break;
    case 10: ret(0x0100); break;
    default: ret(0x08000100);
    }
}
HLE(CFStringConvertEncodingToNSStringEncoding) { ret(arg(0) == 0x0600 ? 1 : arg(0) == 0x0100 ? 10 : 4); }
HLE(CFStringGetFastestEncoding) { ret(0x0100); }
HLE(CFStringGetSystemEncoding) { ret(0); }
HLE(CFStringTransform) { ret(0); }
HLE(CFStringCompare) {
    ret(std::uint64_t(std::int64_t(compareStrings(text16(arg(0)), text16(arg(1)), (arg(2) & 1) ? 1 : 0))));
}
HLE(CFStringFind) {
    const auto pos = text16(arg(0)).find(text16(arg(1)));
    if (pos == std::u16string::npos) { cpu::setX(0, ~0ull); cpu::setX(1, 0); }
    else { cpu::setX(0, pos); cpu::setX(1, text16(arg(1)).size()); }
}
HLE(CFStringHasPrefix) { const auto t = text16(arg(0)), p = text16(arg(1)); ret(t.compare(0, p.size(), p) == 0); }
HLE(CFStringHasSuffix) {
    const auto t = text16(arg(0)), p = text16(arg(1));
    ret(t.size() >= p.size() && t.compare(t.size() - p.size(), p.size(), p) == 0);
}
HLE(CFStringCreateWithFormat) { ret(string(fmtArgs(arg(2), cpu::sp()))); }
HLE(CFStringTokenizerCreate) { ret(0); }
HLE(CFStringTokenizerAdvanceToNextToken) { ret(0); }
HLE(CFStringTokenizerGetCurrentTokenRange) { cpu::setX(0, ~0ull); cpu::setX(1, 0); }
HLE(CFStringTokenizerCopyBestStringLanguage) { ret(string("en")); }
HLE(NSLog) {
    hlog("NSLog", "%s", fmtArgs(hle::arg(0), cpu::sp()).c_str());
}
HLE(NSLogv) { hlog("NSLog", "%s", fmtArgs(hle::arg(0), hle::arg(1)).c_str()); }
HLE(NSStringFromClass) { ret(hle::arg(0) ? string(objc::className(hle::arg(0))) : 0); }
HLE(NSClassFromString) { ret(objc::getClass(text(hle::arg(0)))); }
HLE(NSStringFromSelector) { ret(hle::arg(0) ? string(objc::selName(hle::arg(0))) : 0); }
HLE(NSSelectorFromString) { ret(objc::sel(text(hle::arg(0)))); }
