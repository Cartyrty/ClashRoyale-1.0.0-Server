#pragma once
// Foundation / CoreFoundation over the host object model (toll-free bridged: a CFStringRef is an NSString *).
#include "objc/ObjC.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ns {
using objc::id;
// Strings: UTF-16 contents like NSString.
struct StringData : objc::HostObject {
    std::u16string text;
    std::uint64_t utf8Buffer = 0; // guest copy handed out by -UTF8String
    bool mutableString = false;
};
std::u16string fromUtf8(const std::string &utf8);
std::string toUtf8(const std::u16string &utf16);
id string(const std::string &utf8);          // autoreleased-style (+0, never freed)
id stringU16(const std::u16string &utf16, bool mutableString = false);
std::u16string text16(id string);            // contents of any NSString (constant, host, or nil -> "")
std::string text(id string);                 // UTF-8
bool isString(id object);
std::uint64_t utf8Pointer(id string);        // a stable guest C string
// Numbers.
struct NumberData : objc::HostObject {
    enum Kind { Int, Double, Bool } kind = Int;
    std::int64_t i = 0;
    double d = 0;
    char objcType = 'q';
};
id number(std::int64_t value, char type = 'q');
id numberDouble(double value, char type = 'd');
id numberBool(bool value);
bool isNumber(id object);
std::int64_t intValue(id object);
double doubleValue(id object);
// Collections.
struct ArrayData : objc::HostObject {
    std::vector<id> items;
    bool mutableArray = false;
    std::uint64_t mutations = 0; // guest word for fast enumeration
};
struct DictData : objc::HostObject {
    std::vector<std::pair<id, id>> items;
    bool mutableDict = false;
    std::uint64_t mutations = 0;
};
struct SetData : objc::HostObject {
    std::vector<id> items;
    bool mutableSet = false;
    std::uint64_t mutations = 0;
};
id array(const std::vector<id> &items, bool mutableArray = false);
id dictionary(const std::vector<std::pair<id, id>> &items, bool mutableDict = false);
id set(const std::vector<id> &items, bool mutableSet = false);
std::vector<id> arrayItems(id array);
id dictGet(id dict, id key);
void dictSet(id dict, id key, id value);
bool objectsEqual(id a, id b);
std::uint64_t objectHash(id object);
// Data: bytes live in guest memory.
struct DataBytes : objc::HostObject {
    std::uint64_t bytes = 0, length = 0, capacity = 0;
    bool mutableData = false, freeWhenDone = true;
};
id data(const void *bytes, std::size_t length, bool mutableData = false);
id dataFromGuest(std::uint64_t bytes, std::size_t length, bool copy = true, bool mutableData = false);
std::uint64_t dataBytes(id data);
std::uint64_t dataLength(id data);
// Other.
id null();
std::string describe(id object);           // -description as UTF-8
id dateWithTimeIntervalSinceReferenceDate(double seconds);
double now();                              // seconds since 2001-01-01 (CFAbsoluteTime)
std::string className(id object);
// Notifications.
void postNotification(id name, id object, id userInfo = 0);
} // namespace ns
