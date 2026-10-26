#pragma once
// The original Clash Royale 1.0.0 library (the arm64 iOS executable) running under the HLE core. The server calls its
// functions by their mangled names, so game logic, encodings and checksums are exactly the client's ("the library is
// the absolute truth"). Every function address comes from the library's own symbol table; the only hardcoded numbers
// are object field and virtual table offsets, and those stay inside the Logic* and message classes that use them.
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

namespace library {
// An address in the library's memory (an object, a String, a buffer).
using Ptr = std::uint64_t;

// Virtual table slots shared by the library's polymorphic objects.
namespace vtable {
constexpr int kDeletingDestructor = 0x08;
} // namespace vtable

Ptr symbol(const char *mangledName); // fatal when the library lacks it
Ptr call(const char *mangledName, std::initializer_list<std::uint64_t> arguments = {});
Ptr virtualCall(Ptr object, int vtableOffset, std::initializer_list<std::uint64_t> extraArguments = {});
void deleteObject(Ptr object); // the virtual deleting destructor; nothing for 0

// Memory.
Ptr allocate(std::size_t size); // zeroed
void release(Ptr memory);
Ptr copyBytes(const std::vector<std::uint8_t> &bytes); // a new buffer (one byte longer, zero-terminated)
std::vector<std::uint8_t> readBytes(Ptr address, std::size_t length);
std::int32_t readInt(Ptr address);
void writeInt(Ptr address, std::int32_t value);
Ptr readPointer(Ptr address);
void writePointer(Ptr address, Ptr value);

// titan String: length +0, byte count +4, the characters inline at +8 when they fit in 8 bytes with the terminator,
// otherwise a pointer at +8.
Ptr newString(const std::string &text);
std::string readString(Ptr string);
std::string readStringPointer(Ptr stringPointerField); // a String* field: "" when it is null

// LogicLong: { int high; int low; }.
Ptr newLogicLong(std::int64_t value);
std::int64_t readLogicLong(Ptr logicLong);            // 0 for a null pointer
void writeLogicLong(Ptr address, std::int64_t value); // in place (a LogicLong member, not a pointer)

// Owns a polymorphic library object (a LogicCommand, a LogicBattleEvent, ...): deleted through its virtual
// destructor unless released.
class Object {
public:
    Object() = default;
    explicit Object(Ptr object) : object_(object) {}
    Object(Object &&other) noexcept : object_(other.release()) {}
    Object &operator=(Object &&other) noexcept {
        if (this != &other) reset(other.release());
        return *this;
    }
    Object(const Object &) = delete;
    Object &operator=(const Object &) = delete;
    ~Object() { deleteObject(object_); }

    Ptr get() const { return object_; }
    Ptr release() {
        const auto object = object_;
        object_ = 0;
        return object;
    }
    void reset(Ptr object = 0) {
        deleteObject(object_);
        object_ = object;
    }
    explicit operator bool() const { return object_ != 0; }

private:
    Ptr object_ = 0;
};

// LogicArrayList<T>: data +0, capacity +8, count +0xc.
int arrayCount(Ptr list);           // 0 for a null list
Ptr arrayItem(Ptr list, int index); // LogicArrayList<T*>
void setArrayItem(Ptr list, int index, Ptr value);
std::int32_t arrayIntItem(Ptr list, int index); // LogicArrayList<int>
} // namespace library
