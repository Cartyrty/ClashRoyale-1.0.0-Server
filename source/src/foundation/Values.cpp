// NSNumber / CFNumber / CFBoolean, NSValue, NSNull, NSData / CFData, CFRetain / CFRelease / CFGetTypeID.
#include "foundation/Foundation.h"
#include "Log.h"
#include "Memory.h"
#include "libc/Files.h"
#include <filesystem>
#include <fstream>

namespace ns {
using objc::marg;
using objc::self;
using hle::ret;
namespace {
NumberData *num(id n) { return objc::host<NumberData>(n); }
id attachNumber(id object, NumberData::Kind kind, std::int64_t i, double d, char type) {
    auto n = std::make_unique<NumberData>();
    n->kind = kind;
    n->i = i;
    n->d = d;
    n->objcType = type;
    objc::setHostObject(object, std::move(n));
    return object;
}
id g_true = 0, g_false = 0, g_null = 0;
} // namespace
id number(std::int64_t v, char type) { return attachNumber(objc::allocInstance(objc::getClass("NSNumber")), NumberData::Int, v, double(v), type); }
id numberDouble(double v, char type) {
    return attachNumber(objc::allocInstance(objc::getClass("NSNumber")), NumberData::Double, std::int64_t(v), v, type);
}
id numberBool(bool v) {
    id &slot = v ? g_true : g_false;
    if (!slot) {
        // Permanent booleans (kCFBooleanTrue/False) live in the HLE region so they are never released.
        slot = mem::hleAlloc(16);
        mem::w64(slot, objc::getClass("NSNumber"));
        attachNumber(slot, NumberData::Bool, v, v, 'c');
    }
    return slot;
}
bool isNumber(id o) { return num(o) != nullptr; }
std::int64_t intValue(id o) {
    if (auto *n = num(o)) return n->kind == NumberData::Double ? std::int64_t(n->d) : n->i;
    if (isString(o)) return std::atoll(text(o).c_str());
    return 0;
}
double doubleValue(id o) {
    if (auto *n = num(o)) return n->kind == NumberData::Double ? n->d : double(n->i);
    if (isString(o)) return std::atof(text(o).c_str());
    return 0;
}
id null() {
    if (!g_null) {
        g_null = mem::hleAlloc(16);
        mem::w64(g_null, objc::getClass("NSNull"));
    }
    return g_null;
}

// NSData
namespace {
DataBytes *bytesOf(id d) { return objc::host<DataBytes>(d); }
id attachData(id object, std::uint64_t guestBytes, std::uint64_t length, bool copy, bool m) {
    auto d = std::make_unique<DataBytes>();
    d->mutableData = m;
    d->length = length;
    if (copy || m) {
        d->capacity = std::max<std::uint64_t>(length, 16);
        d->bytes = mem::alloc(std::size_t(d->capacity));
        if (length && guestBytes) std::memcpy(mem::base() + d->bytes, mem::base() + guestBytes, std::size_t(length));
        else if (length) std::memset(mem::base() + d->bytes, 0, std::size_t(length));
    } else {
        d->bytes = guestBytes;
        d->capacity = length;
        d->freeWhenDone = false;
    }
    objc::setHostObject(object, std::move(d));
    return object;
}
void grow(DataBytes *d, std::uint64_t length) {
    if (length > d->capacity) {
        const auto cap = std::max<std::uint64_t>(length, d->capacity * 2);
        const auto fresh = mem::calloc(std::size_t(cap));
        if (d->length) std::memcpy(mem::base() + fresh, mem::base() + d->bytes, std::size_t(d->length));
        d->bytes = fresh;
        d->capacity = cap;
    }
    if (length > d->length) std::memset(mem::base() + d->bytes + d->length, 0, std::size_t(length - d->length));
    d->length = length;
}
} // namespace
id data(const void *bytes, std::size_t length, bool m) {
    const auto object = attachData(objc::allocInstance(objc::getClass(m ? "NSMutableData" : "NSData")), 0, length, true, m);
    if (length) std::memcpy(mem::base() + bytesOf(object)->bytes, bytes, length);
    return object;
}
id dataFromGuest(std::uint64_t bytes, std::size_t length, bool copy, bool m) {
    return attachData(objc::allocInstance(objc::getClass(m ? "NSMutableData" : "NSData")), bytes, length, copy, m);
}
std::uint64_t dataBytes(id d) { auto *b = bytesOf(d); return b ? b->bytes : 0; }
std::uint64_t dataLength(id d) { auto *b = bytesOf(d); return b ? b->length : 0; }
} // namespace ns

using namespace ns;
using objc::marg;
using objc::self;
using hle::ret;
using hle::arg;

// ======================================== NSNumber ========================================
OBJC_CLASS(NSValue, NSObject);
OBJC_CLASS(NSNumber, NSValue);
#define NUMBER_CTOR(sel, idn, expr, type)                                                                              \
    OBJC_CLASS_METHOD(NSNumber, sel, idn) { ret(number(expr, type)); }
NUMBER_CTOR("numberWithInt:", withInt, std::int32_t(marg(0)), 'i')
NUMBER_CTOR("numberWithUnsignedInt:", withUnsignedInt, std::uint32_t(marg(0)), 'I')
NUMBER_CTOR("numberWithInteger:", withInteger, std::int64_t(marg(0)), 'q')
NUMBER_CTOR("numberWithUnsignedInteger:", withUnsignedInteger, std::int64_t(marg(0)), 'Q')
NUMBER_CTOR("numberWithLong:", withLong, std::int64_t(marg(0)), 'q')
NUMBER_CTOR("numberWithUnsignedLong:", withUnsignedLong, std::int64_t(marg(0)), 'Q')
NUMBER_CTOR("numberWithLongLong:", withLongLong, std::int64_t(marg(0)), 'q')
NUMBER_CTOR("numberWithUnsignedLongLong:", withUnsignedLongLong, std::int64_t(marg(0)), 'Q')
NUMBER_CTOR("numberWithShort:", withShort, std::int16_t(marg(0)), 's')
NUMBER_CTOR("numberWithUnsignedShort:", withUnsignedShort, std::uint16_t(marg(0)), 'S')
NUMBER_CTOR("numberWithChar:", withChar, std::int8_t(marg(0)), 'c')
NUMBER_CTOR("numberWithUnsignedChar:", withUnsignedChar, std::uint8_t(marg(0)), 'C')
OBJC_CLASS_METHOD(NSNumber, "numberWithBool:", withBool) { ret(numberBool((marg(0) & 0xFF) != 0)); }
OBJC_CLASS_METHOD(NSNumber, "numberWithFloat:", withFloat) { ret(numberDouble(cpu::s(0), 'f')); }
OBJC_CLASS_METHOD(NSNumber, "numberWithDouble:", withDouble) { ret(numberDouble(cpu::d(0), 'd')); }
OBJC_CLASS_METHOD(NSNumber, "alloc", alloc) { ret(objc::allocInstance(self())); }
#define NUMBER_INIT(sel, idn, expr, type)                                                                              \
    OBJC_METHOD(NSNumber, sel, idn) { ret(attachNumber(self(), NumberData::Int, expr, 0, type)); }
NUMBER_INIT("initWithInt:", initWithInt, std::int32_t(marg(0)), 'i')
NUMBER_INIT("initWithUnsignedInt:", initWithUnsignedInt, std::uint32_t(marg(0)), 'I')
NUMBER_INIT("initWithInteger:", initWithInteger, std::int64_t(marg(0)), 'q')
NUMBER_INIT("initWithLongLong:", initWithLongLong, std::int64_t(marg(0)), 'q')
NUMBER_INIT("initWithUnsignedLongLong:", initWithUnsignedLongLong, std::int64_t(marg(0)), 'Q')
NUMBER_INIT("initWithUnsignedInteger:", initWithUnsignedInteger, std::int64_t(marg(0)), 'Q')
OBJC_METHOD(NSNumber, "initWithBool:", initWithBool) { ret(attachNumber(self(), NumberData::Bool, (marg(0) & 0xFF) != 0, 0, 'c')); }
OBJC_METHOD(NSNumber, "initWithFloat:", initWithFloat) { ret(attachNumber(self(), NumberData::Double, 0, cpu::s(0), 'f')); }
OBJC_METHOD(NSNumber, "initWithDouble:", initWithDouble) { ret(attachNumber(self(), NumberData::Double, 0, cpu::d(0), 'd')); }
OBJC_METHOD(NSNumber, "intValue", intValue_) { ret(std::uint64_t(std::int64_t(std::int32_t(intValue(self()))))); }
OBJC_METHOD(NSNumber, "unsignedIntValue", unsignedIntValue) { ret(std::uint32_t(intValue(self()))); }
OBJC_METHOD(NSNumber, "integerValue", integerValue) { ret(std::uint64_t(intValue(self()))); }
OBJC_METHOD(NSNumber, "unsignedIntegerValue", unsignedIntegerValue) { ret(std::uint64_t(intValue(self()))); }
OBJC_METHOD(NSNumber, "longValue", longValue) { ret(std::uint64_t(intValue(self()))); }
OBJC_METHOD(NSNumber, "longLongValue", longLongValue) { ret(std::uint64_t(intValue(self()))); }
OBJC_METHOD(NSNumber, "unsignedLongLongValue", unsignedLongLongValue) { ret(std::uint64_t(intValue(self()))); }
OBJC_METHOD(NSNumber, "shortValue", shortValue) { ret(std::uint64_t(std::int64_t(std::int16_t(intValue(self()))))); }
OBJC_METHOD(NSNumber, "charValue", charValue) { ret(std::uint64_t(std::int64_t(std::int8_t(intValue(self()))))); }
OBJC_METHOD(NSNumber, "unsignedCharValue", unsignedCharValue) { ret(std::uint8_t(intValue(self()))); }
OBJC_METHOD(NSNumber, "boolValue", boolValue) { ret(doubleValue(self()) != 0); }
OBJC_METHOD(NSNumber, "floatValue", floatValue) { hle::retFloat(float(doubleValue(self()))); }
OBJC_METHOD(NSNumber, "doubleValue", doubleValue_) { hle::retDouble(doubleValue(self())); }
OBJC_METHOD(NSNumber, "stringValue", stringValue) { ret(objc::send(self(), "description")); }
OBJC_METHOD(NSNumber, "description", description) {
    auto *n = objc::host<NumberData>(self());
    if (!n) return ret(string("0"));
    if (n->kind == NumberData::Double) {
        char b[64];
        std::snprintf(b, sizeof b, "%.17g", n->d);
        return ret(string(b));
    }
    ret(string(std::to_string(n->i)));
}
OBJC_METHOD(NSNumber, "objCType", objCType) {
    static std::uint64_t types[128] = {};
    auto *n = objc::host<NumberData>(self());
    const char t = n ? n->objcType : 'q';
    if (!types[int(t)]) types[int(t)] = mem::hleString(std::string(1, t));
    ret(types[int(t)]);
}
OBJC_METHOD(NSNumber, "isEqual:", isEqual) { ret(isNumber(marg(0)) && doubleValue(self()) == doubleValue(marg(0))); }
OBJC_METHOD(NSNumber, "isEqualToNumber:", isEqualToNumber) { ret(isNumber(marg(0)) && doubleValue(self()) == doubleValue(marg(0))); }
OBJC_METHOD(NSNumber, "hash", hash) { ret(std::uint64_t(intValue(self()))); }
OBJC_METHOD(NSNumber, "compare:", compare) {
    const double a = doubleValue(self()), b = doubleValue(marg(0));
    ret(std::uint64_t(std::int64_t(a < b ? -1 : a > b ? 1 : 0)));
}
OBJC_METHOD(NSNumber, "copyWithZone:", copyWithZone) { objc::retain(self()); ret(self()); }
static hle::DataRegistrar d_true("_kCFBooleanTrue", [] { const auto p = mem::hleAlloc(8); mem::w64(p, numberBool(true)); return p; });
static hle::DataRegistrar d_false("_kCFBooleanFalse", [] { const auto p = mem::hleAlloc(8); mem::w64(p, numberBool(false)); return p; });
HLE(CFBooleanGetValue) { hle::ret(doubleValue(arg(0)) != 0); }
// CFNumberCreate(alloc, type, valuePtr): kCFNumberSInt32Type 3, SInt64 4, Float32 5, Float64 6, Int 9, Float 12,
// Double 13, CFIndex 14, NSInteger 15, CGFloat 16.
HLE(CFNumberCreate) {
    const auto type = arg(1), p = arg(2);
    switch (type) {
    case 1: case 7: ret(number(mem::read<std::int8_t>(p), 'c')); break;
    case 2: case 8: ret(number(mem::read<std::int16_t>(p), 's')); break;
    case 3: case 9: ret(number(std::int32_t(mem::r32(p)), 'i')); break;
    case 5: case 12: ret(numberDouble(mem::read<float>(p), 'f')); break;
    case 6: case 13: case 16: ret(numberDouble(mem::read<double>(p), 'd')); break;
    default: ret(number(std::int64_t(mem::r64(p)), 'q'));
    }
}
HLE(CFNumberGetValue) {
    const auto type = arg(1), p = arg(2);
    switch (type) {
    case 1: case 7: mem::write<std::int8_t>(p, std::int8_t(intValue(arg(0)))); break;
    case 2: case 8: mem::write<std::int16_t>(p, std::int16_t(intValue(arg(0)))); break;
    case 3: case 9: mem::w32(p, std::uint32_t(intValue(arg(0)))); break;
    case 5: case 12: mem::write<float>(p, float(doubleValue(arg(0)))); break;
    case 6: case 13: case 16: mem::write<double>(p, doubleValue(arg(0))); break;
    default: mem::w64(p, std::uint64_t(intValue(arg(0))));
    }
    ret(1);
}

// ======================================== NSValue ========================================
struct ValueData : objc::HostObject {
    std::vector<std::uint8_t> bytes;
    std::string type;
};
static id makeValue(const void *bytes, std::size_t size, const std::string &type) {
    const auto object = objc::allocInstance(objc::getClass("NSValue"));
    auto v = std::make_unique<ValueData>();
    v->bytes.assign(static_cast<const std::uint8_t *>(bytes), static_cast<const std::uint8_t *>(bytes) + size);
    v->type = type;
    objc::setHostObject(object, std::move(v));
    return object;
}
OBJC_CLASS_METHOD(NSValue, "valueWithCGRect:", valueWithCGRect) {
    const double r[4] = {cpu::d(0), cpu::d(1), cpu::d(2), cpu::d(3)};
    ret(makeValue(r, sizeof r, "{CGRect={CGPoint=dd}{CGSize=dd}}"));
}
OBJC_CLASS_METHOD(NSValue, "valueWithCGPoint:", valueWithCGPoint) {
    const double r[2] = {cpu::d(0), cpu::d(1)};
    ret(makeValue(r, sizeof r, "{CGPoint=dd}"));
}
OBJC_CLASS_METHOD(NSValue, "valueWithCGSize:", valueWithCGSize) {
    const double r[2] = {cpu::d(0), cpu::d(1)};
    ret(makeValue(r, sizeof r, "{CGSize=dd}"));
}
OBJC_CLASS_METHOD(NSValue, "valueWithPointer:", valueWithPointer) { const auto p = marg(0); ret(makeValue(&p, 8, "^v")); }
OBJC_CLASS_METHOD(NSValue, "valueWithNonretainedObject:", valueWithNonretainedObject) { const auto p = marg(0); ret(makeValue(&p, 8, "@")); }
OBJC_CLASS_METHOD(NSValue, "valueWithBytes:objCType:", valueWithBytes) {
    // Size from the type is not computed; 32 bytes covers the structs passed this way (CGRect, CGAffineTransform halves).
    ret(makeValue(mem::base() + marg(0), 32, mem::cstring(marg(1))));
}
static double valueDouble(int i) {
    auto *v = objc::host<ValueData>(self());
    double d = 0;
    if (v && v->bytes.size() >= std::size_t(8 * (i + 1))) std::memcpy(&d, v->bytes.data() + 8 * i, 8);
    return d;
}
OBJC_METHOD(NSValue, "CGRectValue", CGRectValue) { for (int i = 0; i < 4; ++i) cpu::setD(i, valueDouble(i)); }
OBJC_METHOD(NSValue, "CGPointValue", CGPointValue) { for (int i = 0; i < 2; ++i) cpu::setD(i, valueDouble(i)); }
OBJC_METHOD(NSValue, "CGSizeValue", CGSizeValue) { for (int i = 0; i < 2; ++i) cpu::setD(i, valueDouble(i)); }
OBJC_METHOD(NSValue, "pointerValue", pointerValue) {
    auto *v = objc::host<ValueData>(self());
    std::uint64_t p = 0;
    if (v && v->bytes.size() >= 8) std::memcpy(&p, v->bytes.data(), 8);
    ret(p);
}
OBJC_METHOD(NSValue, "nonretainedObjectValue", nonretainedObjectValue) {
    auto *v = objc::host<ValueData>(self());
    std::uint64_t p = 0;
    if (v && v->bytes.size() >= 8) std::memcpy(&p, v->bytes.data(), 8);
    ret(p);
}
OBJC_METHOD(NSValue, "getValue:", getValue) {
    if (auto *v = objc::host<ValueData>(self())) std::memcpy(mem::base() + marg(0), v->bytes.data(), v->bytes.size());
}

// ======================================== NSNull ========================================
OBJC_CLASS(NSNull, NSObject);
OBJC_CLASS_METHOD(NSNull, "null", null_) { ret(null()); }
OBJC_METHOD(NSNull, "description", description) { ret(string("<null>")); }
static hle::DataRegistrar d_kCFNull("_kCFNull", [] { const auto p = mem::hleAlloc(8); mem::w64(p, null()); return p; });

// ======================================== NSData ========================================
OBJC_CLASS(NSData, NSObject);
OBJC_CLASS(NSMutableData, NSData);
static bool mutableDataClass() { return objc::isSubclass(self(), objc::getClass("NSMutableData")); }
static bool readFile(id path, std::string &out) { return files::readWholeFile(text(path), out); }
OBJC_CLASS_METHOD(NSData, "alloc", alloc) { ret(attachData(objc::allocInstance(self()), 0, 0, true, mutableDataClass())); }
OBJC_CLASS_METHOD(NSData, "data", data_) { ret(dataFromGuest(0, 0, true, mutableDataClass())); }
OBJC_CLASS_METHOD(NSData, "dataWithBytes:length:", dataWithBytes) { ret(dataFromGuest(marg(0), std::size_t(marg(1)), true, mutableDataClass())); }
OBJC_CLASS_METHOD(NSData, "dataWithBytesNoCopy:length:", dataWithBytesNoCopy) {
    ret(dataFromGuest(marg(0), std::size_t(marg(1)), mutableDataClass(), mutableDataClass()));
}
OBJC_CLASS_METHOD(NSData, "dataWithBytesNoCopy:length:freeWhenDone:", dataWithBytesNoCopyFree) {
    ret(dataFromGuest(marg(0), std::size_t(marg(1)), mutableDataClass(), mutableDataClass()));
}
OBJC_CLASS_METHOD(NSData, "dataWithData:", dataWithData) {
    ret(dataFromGuest(dataBytes(marg(0)), std::size_t(dataLength(marg(0))), true, mutableDataClass()));
}
OBJC_CLASS_METHOD(NSData, "dataWithContentsOfFile:", dataWithContentsOfFile) {
    std::string bytes;
    if (!readFile(marg(0), bytes)) return ret(0);
    ret(data(bytes.data(), bytes.size(), mutableDataClass()));
}
OBJC_CLASS_METHOD(NSData, "dataWithContentsOfFile:options:error:", dataWithContentsOfFileOptions) {
    std::string bytes;
    if (!readFile(marg(0), bytes)) { if (marg(2)) mem::w64(marg(2), 0); return ret(0); }
    ret(data(bytes.data(), bytes.size(), mutableDataClass()));
}
OBJC_CLASS_METHOD(NSData, "dataWithContentsOfURL:", dataWithContentsOfURL) {
    std::string bytes;
    if (!readFile(objc::send(marg(0), "path"), bytes)) return ret(0);
    ret(data(bytes.data(), bytes.size(), mutableDataClass()));
}
OBJC_CLASS_METHOD(NSMutableData, "dataWithLength:", dataWithLength) { ret(dataFromGuest(0, std::size_t(marg(0)), true, true)); }
OBJC_CLASS_METHOD(NSMutableData, "dataWithCapacity:", dataWithCapacity) { ret(dataFromGuest(0, 0, true, true)); }
OBJC_METHOD(NSData, "init", init) { ret(self()); }
OBJC_METHOD(NSData, "initWithBytes:length:", initWithBytes) { ret(attachData(self(), marg(0), marg(1), true, bytesOf(self()) && bytesOf(self())->mutableData)); }
OBJC_METHOD(NSData, "initWithBytesNoCopy:length:", initWithBytesNoCopy) { ret(attachData(self(), marg(0), marg(1), false, false)); }
OBJC_METHOD(NSData, "initWithBytesNoCopy:length:freeWhenDone:", initWithBytesNoCopyFree) { ret(attachData(self(), marg(0), marg(1), false, false)); }
OBJC_METHOD(NSData, "initWithData:", initWithData) {
    ret(attachData(self(), dataBytes(marg(0)), dataLength(marg(0)), true, bytesOf(self()) && bytesOf(self())->mutableData));
}
OBJC_METHOD(NSData, "initWithContentsOfFile:", initWithContentsOfFile) {
    std::string bytes;
    if (!readFile(marg(0), bytes)) return ret(0);
    attachData(self(), 0, bytes.size(), true, false);
    std::memcpy(mem::base() + bytesOf(self())->bytes, bytes.data(), bytes.size());
    ret(self());
}
OBJC_METHOD(NSMutableData, "initWithLength:", initWithLength) { ret(attachData(self(), 0, marg(0), true, true)); }
OBJC_METHOD(NSMutableData, "initWithCapacity:", initWithCapacity) { ret(attachData(self(), 0, 0, true, true)); }
OBJC_METHOD(NSData, "bytes", bytes) { ret(dataBytes(self())); }
OBJC_METHOD(NSData, "length", length) { ret(dataLength(self())); }
OBJC_METHOD(NSData, "getBytes:length:", getBytesLength) {
    std::memcpy(mem::base() + marg(0), mem::base() + dataBytes(self()), std::size_t(std::min(marg(1), dataLength(self()))));
}
OBJC_METHOD(NSData, "getBytes:range:", getBytesRange) {
    std::memcpy(mem::base() + marg(0), mem::base() + dataBytes(self()) + marg(1), std::size_t(marg(2)));
}
OBJC_METHOD(NSData, "subdataWithRange:", subdataWithRange) { ret(dataFromGuest(dataBytes(self()) + marg(0), std::size_t(marg(1)))); }
OBJC_METHOD(NSData, "isEqualToData:", isEqualToData) {
    ret(dataLength(self()) == dataLength(marg(0)) &&
        !std::memcmp(mem::base() + dataBytes(self()), mem::base() + dataBytes(marg(0)), std::size_t(dataLength(self()))));
}
OBJC_METHOD(NSData, "isEqual:", isEqual) {
    ret(objc::host<DataBytes>(marg(0)) && dataLength(self()) == dataLength(marg(0)) &&
        !std::memcmp(mem::base() + dataBytes(self()), mem::base() + dataBytes(marg(0)), std::size_t(dataLength(self()))));
}
OBJC_METHOD(NSData, "copyWithZone:", copyWithZone) { ret(dataFromGuest(dataBytes(self()), std::size_t(dataLength(self())))); }
OBJC_METHOD(NSData, "mutableCopyWithZone:", mutableCopyWithZone) { ret(dataFromGuest(dataBytes(self()), std::size_t(dataLength(self())), true, true)); }
OBJC_METHOD(NSData, "description", description) {
    std::string out = "<";
    const auto b = dataBytes(self()), n = dataLength(self());
    for (std::uint64_t i = 0; i < n; ++i) {
        char h[3];
        std::snprintf(h, sizeof h, "%02x", mem::base()[b + i]);
        out += h;
        if (i % 4 == 3 && i + 1 < n) out += ' ';
    }
    ret(string(out + ">"));
}
static bool writeData(id self_, id path) {
    const auto host = files::hostPath(text(path));
    std::filesystem::create_directories(std::filesystem::u8path(host).parent_path());
    std::ofstream out(std::filesystem::u8path(host), std::ios::binary);
    out.write(reinterpret_cast<char *>(mem::base() + dataBytes(self_)), std::streamsize(dataLength(self_)));
    hlog("file", "write %s (%llu bytes)", text(path).c_str(), (unsigned long long)dataLength(self_));
    return bool(out);
}
OBJC_METHOD(NSData, "writeToFile:atomically:", writeToFile) { ret(writeData(self(), marg(0))); }
OBJC_METHOD(NSData, "writeToFile:options:error:", writeToFileOptions) { ret(writeData(self(), marg(0))); }
OBJC_METHOD(NSData, "writeToURL:atomically:", writeToURL) { ret(writeData(self(), objc::send(marg(0), "path"))); }
static const char *kBase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
OBJC_METHOD(NSData, "base64EncodedStringWithOptions:", base64Encoded) {
    const auto b = dataBytes(self()), n = dataLength(self());
    std::string out;
    for (std::uint64_t i = 0; i < n; i += 3) {
        const std::uint32_t v = (std::uint32_t(mem::base()[b + i]) << 16) | (i + 1 < n ? std::uint32_t(mem::base()[b + i + 1]) << 8 : 0) |
                                (i + 2 < n ? mem::base()[b + i + 2] : 0);
        out += kBase64[(v >> 18) & 63];
        out += kBase64[(v >> 12) & 63];
        out += i + 1 < n ? kBase64[(v >> 6) & 63] : '=';
        out += i + 2 < n ? kBase64[v & 63] : '=';
    }
    ret(string(out));
}
OBJC_METHOD(NSData, "initWithBase64EncodedString:options:", initWithBase64) {
    const auto s = text(marg(0));
    std::string out;
    std::uint32_t buffer = 0;
    int bits = 0;
    for (char c : s) {
        const char *p = std::strchr(kBase64, c);
        if (!p || !c) continue;
        buffer = (buffer << 6) | std::uint32_t(p - kBase64);
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back(char((buffer >> bits) & 0xFF)); }
    }
    attachData(self(), 0, out.size(), true, false);
    std::memcpy(mem::base() + bytesOf(self())->bytes, out.data(), out.size());
    ret(self());
}
OBJC_METHOD(NSMutableData, "mutableBytes", mutableBytes) { ret(dataBytes(self())); }
OBJC_METHOD(NSMutableData, "setLength:", setLength) { if (auto *d = bytesOf(self())) grow(d, marg(0)); }
OBJC_METHOD(NSMutableData, "increaseLengthBy:", increaseLengthBy) { if (auto *d = bytesOf(self())) grow(d, d->length + marg(0)); }
OBJC_METHOD(NSMutableData, "appendBytes:length:", appendBytes) {
    auto *d = bytesOf(self());
    if (!d) return;
    std::vector<std::uint8_t> copy(mem::base() + marg(0), mem::base() + marg(0) + marg(1));
    const auto at = d->length;
    grow(d, d->length + marg(1));
    std::memcpy(mem::base() + d->bytes + at, copy.data(), copy.size());
}
OBJC_METHOD(NSMutableData, "appendData:", appendData) {
    auto *d = bytesOf(self());
    if (!d) return;
    const auto src = dataBytes(marg(0)), n = dataLength(marg(0));
    std::vector<std::uint8_t> copy(mem::base() + src, mem::base() + src + n);
    const auto at = d->length;
    grow(d, d->length + n);
    std::memcpy(mem::base() + d->bytes + at, copy.data(), copy.size());
}
OBJC_METHOD(NSMutableData, "replaceBytesInRange:withBytes:", replaceBytes) {
    auto *d = bytesOf(self());
    if (!d) return;
    if (marg(0) + marg(1) > d->length) grow(d, marg(0) + marg(1));
    std::memmove(mem::base() + d->bytes + marg(0), mem::base() + marg(2), std::size_t(marg(1)));
}
OBJC_METHOD(NSMutableData, "resetBytesInRange:", resetBytes) {
    auto *d = bytesOf(self());
    if (d && marg(0) + marg(1) <= d->length) std::memset(mem::base() + d->bytes + marg(0), 0, std::size_t(marg(1)));
}
HLE(CFDataCreate) { ret(dataFromGuest(arg(1), std::size_t(arg(2)))); }
HLE(CFDataCreateWithBytesNoCopy) { ret(dataFromGuest(arg(1), std::size_t(arg(2)), false)); }
HLE(CFDataCreateMutable) { ret(dataFromGuest(0, 0, true, true)); }
HLE(CFDataGetBytePtr) { ret(dataBytes(arg(0))); }
HLE(CFDataGetMutableBytePtr) { ret(dataBytes(arg(0))); }
HLE(CFDataGetLength) { ret(dataLength(arg(0))); }
HLE(CFDataGetBytes) { std::memcpy(mem::base() + arg(3), mem::base() + dataBytes(arg(0)) + arg(1), std::size_t(arg(2))); }
HLE(CFDataAppendBytes) { objc::send(arg(0), "appendBytes:length:", {arg(1), arg(2)}); }

// ======================================== CFType ========================================
HLE(CFRetain) { objc::retain(arg(0)); ret(arg(0)); }
HLE(CFRelease) { objc::release(arg(0)); }
HLE(CFAutorelease) { objc::autorelease(arg(0)); ret(arg(0)); }
HLE(CFGetRetainCount) { ret(objc::retainCount(arg(0))); }
HLE(CFEqual) { ret(objectsEqual(arg(0), arg(1))); }
HLE(CFHash) { ret(objectHash(arg(0))); }
HLE(CFCopyDescription) { ret(string(describe(arg(0)))); }
// Type ids: string 7, dictionary 18, array 19, boolean 21, number 22, data 20, date 42, null 16, url 29.
static std::uint64_t typeId(id o) {
    if (!o) return 0;
    if (isString(o)) return 7;
    if (objc::host<DictData>(o)) return 18;
    if (objc::host<ArrayData>(o)) return 19;
    if (objc::host<DataBytes>(o)) return 20;
    if (auto *n = objc::host<NumberData>(o)) return n->kind == NumberData::Bool ? 21 : 22;
    if (o == null()) return 16;
    return 1;
}
HLE(CFGetTypeID) { ret(typeId(arg(0))); }
HLE(CFStringGetTypeID) { ret(7); }
HLE(CFDictionaryGetTypeID) { ret(18); }
HLE(CFArrayGetTypeID) { ret(19); }
HLE(CFDataGetTypeID) { ret(20); }
HLE(CFBooleanGetTypeID) { ret(21); }
HLE(CFNumberGetTypeID) { ret(22); }
HLE(CFAllocatorCreate) { ret(0); }
static hle::DataRegistrar d_allocDefault("_kCFAllocatorDefault", [] { return mem::hleAlloc(8); });
static hle::DataRegistrar d_allocNull("_kCFAllocatorNull", [] { return mem::hleAlloc(8); });
static hle::DataRegistrar d_allocMalloc("_kCFAllocatorMalloc", [] { return mem::hleAlloc(8); });
static hle::DataRegistrar d_allocSystem("_kCFAllocatorSystemDefault", [] { return mem::hleAlloc(8); });
