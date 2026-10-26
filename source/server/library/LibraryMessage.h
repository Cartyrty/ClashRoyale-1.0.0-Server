#pragma once
// A message object of the library (a PiranhaMessage subclass made by its LogicScrollMessageFactory). Messages whose
// encoding the library owns - LoginMessage, OwnHomeDataMessage, the sector messages - are encoded and decoded with
// these, so the server writes and reads exactly the client's format. Fields are reached by their offsets in the
// library object (PiranhaMessage: version +8, ByteStream +0x10; subclass fields from +0x44).
#include "library/Library.h"

#include <cstdint>
#include <vector>

namespace library {
class LibraryMessage {
public:
    // A new, empty message of the type (its constructor's defaults); fatal for a type the library does not know.
    static LibraryMessage create(int messageType);
    // A message decoded from a received payload.
    static LibraryMessage decode(int messageType, const std::vector<std::uint8_t> &payload);

    LibraryMessage(LibraryMessage &&other) noexcept;
    LibraryMessage &operator=(LibraryMessage &&other) noexcept;
    LibraryMessage(const LibraryMessage &) = delete;
    LibraryMessage &operator=(const LibraryMessage &) = delete;
    ~LibraryMessage(); // the message's destructor also frees the objects its fields own

    std::vector<std::uint8_t> encode();

    std::int32_t readInt(int offset) const;
    void writeInt(int offset, std::int32_t value);
    Ptr readPointer(int offset) const;
    void writePointer(int offset, Ptr value); // the message then owns the object
    Ptr takePointer(int offset);              // reads the field and clears it: the caller owns the object
    Ptr get() const { return message_; }

private:
    explicit LibraryMessage(Ptr message) : message_(message) {}
    Ptr message_ = 0;
};
} // namespace library
