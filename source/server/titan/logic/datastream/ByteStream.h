#pragma once
// A ByteStream as the client has it (writeVInt, writeString, readBoolean, ...), backed by the library's own ByteStream
// object so every VInt, packed Boolean, String and Long has exactly the client's wire format. Library encoders
// (LogicClientHome::encode, LogicGameMode::encode, ...) write into the same object through getLibraryStream().
#include "library/Library.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace server {
class ByteStream {
public:
    static constexpr int kMaxStringLength = 900000; // ByteStream::readString's limit

    ByteStream();                                                // empty, for writing
    explicit ByteStream(const std::vector<std::uint8_t> &bytes); // reading the bytes from the start
    ByteStream(ByteStream &&other) noexcept;
    ByteStream &operator=(ByteStream &&other) noexcept;
    ByteStream(const ByteStream &) = delete;
    ByteStream &operator=(const ByteStream &) = delete;
    ~ByteStream();

    void writeVInt(int value);
    void writeInt(int value);
    void writeBoolean(bool value);
    void writeString(const std::optional<std::string> &value); // null when empty
    void writeStringReference(const std::string &value);
    void writeLong(std::int64_t value);                                   // ChecksumEncoder::writeLong: two Ints
    void writeBytesWithoutLength(const std::vector<std::uint8_t> &bytes); // raw, without a length
    void writeBytes(const std::vector<std::uint8_t> &bytes);              // Int length, then the bytes
    void writeByte(std::uint8_t value);

    int readVInt();
    int readInt();
    bool readBoolean();
    std::optional<std::string> readString(int maxLength = kMaxStringLength); // null for a negative length
    std::int64_t readLong();
    std::uint8_t readByte();
    std::vector<std::uint8_t> readBytes(int length, int maxLength = kMaxStringLength); // without a length
    int readBytesLength();
    bool isAtEnd() const;

    std::vector<std::uint8_t> getByteArray() const;            // everything written (or set)
    void setByteArray(const std::vector<std::uint8_t> &bytes); // replaces the content and reads it from the start
    library::Ptr getLibraryStream() const { return stream_; }

private:
    library::Ptr stream_ = 0;
};
} // namespace server
