#include "titan/logic/datastream/ByteStream.h"

namespace server {
using library::call;

namespace {
constexpr std::size_t kLibraryStreamSize = 0x40;
constexpr int kInitialCapacity = 128;
} // namespace

ByteStream::ByteStream() : stream_(library::allocate(kLibraryStreamSize)) {
    call("__ZN10ByteStreamC1Ei", {stream_, kInitialCapacity});
}

ByteStream::ByteStream(const std::vector<std::uint8_t> &bytes) : stream_(library::allocate(kLibraryStreamSize)) {
    call("__ZN10ByteStreamC1Ei", {stream_, 0});
    setByteArray(bytes);
}

ByteStream::ByteStream(ByteStream &&other) noexcept : stream_(other.stream_) { other.stream_ = 0; }

ByteStream &ByteStream::operator=(ByteStream &&other) noexcept {
    if (this != &other) {
        this->~ByteStream();
        stream_ = other.stream_;
        other.stream_ = 0;
    }
    return *this;
}

ByteStream::~ByteStream() {
    if (!stream_) return;
    call("__ZN10ByteStream8destructEv", {stream_});
    library::release(stream_);
}

void ByteStream::writeVInt(int value) {
    call("__ZN10ByteStream9writeVIntEi", {stream_, std::uint64_t(std::uint32_t(value))});
}
void ByteStream::writeInt(int value) {
    call("__ZN10ByteStream8writeIntEi", {stream_, std::uint64_t(std::uint32_t(value))});
}
void ByteStream::writeBoolean(bool value) { call("__ZN10ByteStream12writeBooleanEb", {stream_, value ? 1u : 0u}); }

void ByteStream::writeString(const std::optional<std::string> &value) {
    call("__ZN10ByteStream11writeStringEP6String", {stream_, value ? library::newString(*value) : 0});
}

void ByteStream::writeStringReference(const std::string &value) {
    call("__ZN10ByteStream20writeStringReferenceERK6String", {stream_, library::newString(value)});
}

void ByteStream::writeLong(std::int64_t value) {
    call("__ZN15ChecksumEncoder9writeLongEPK9LogicLong", {stream_, library::newLogicLong(value)});
}

void ByteStream::writeBytesWithoutLength(const std::vector<std::uint8_t> &bytes) {
    if (bytes.empty()) return;
    const auto buffer = library::copyBytes(bytes);
    call("__ZN10ByteStream23writeBytesWithoutLengthEPci", {stream_, buffer, bytes.size()});
    library::release(buffer);
}

void ByteStream::writeBytes(const std::vector<std::uint8_t> &bytes) {
    const auto buffer = library::copyBytes(bytes);
    call("__ZN10ByteStream10writeBytesEPci", {stream_, buffer, bytes.size()});
    library::release(buffer);
}

void ByteStream::writeByte(std::uint8_t value) { call("__ZN10ByteStream9writeByteEc", {stream_, value}); }

std::uint8_t ByteStream::readByte() { return std::uint8_t(call("__ZN10ByteStream8readByteEv", {stream_})); }

// ByteStream::readBytes returns a new[] buffer (null when the length is invalid).
std::vector<std::uint8_t> ByteStream::readBytes(int length, int maxLength) {
    const auto buffer = call("__ZN10ByteStream9readBytesEii",
                             {stream_, std::uint64_t(std::uint32_t(length)), std::uint64_t(std::uint32_t(maxLength))});
    if (!buffer || length <= 0) return {};
    auto bytes = library::readBytes(buffer, std::size_t(length));
    library::release(buffer);
    return bytes;
}

int ByteStream::readBytesLength() { return int(call("__ZN10ByteStream15readBytesLengthEv", {stream_})); }

int ByteStream::readVInt() { return int(call("__ZN10ByteStream8readVIntEv", {stream_})); }
int ByteStream::readInt() { return int(call("__ZN10ByteStream7readIntEv", {stream_})); }
bool ByteStream::readBoolean() { return (call("__ZN10ByteStream11readBooleanEv", {stream_}) & 0xff) != 0; }

std::optional<std::string> ByteStream::readString(int maxLength) {
    const auto string = call("__ZN10ByteStream10readStringEi", {stream_, std::uint64_t(std::uint32_t(maxLength))});
    if (!string) return std::nullopt;
    return library::readString(string);
}

std::int64_t ByteStream::readLong() { return library::readLogicLong(call("__ZN10ByteStream8readLongEv", {stream_})); }
bool ByteStream::isAtEnd() const { return (call("__ZNK10ByteStream7isAtEndEv", {stream_}) & 0xff) != 0; }

std::vector<std::uint8_t> ByteStream::getByteArray() const {
    const int length = int(call("__ZNK10ByteStream9getLengthEv", {stream_}));
    const auto data = call("__ZNK10ByteStream12getByteArrayEv", {stream_});
    if (length <= 0 || !data) return {};
    return library::readBytes(data, std::size_t(length));
}

void ByteStream::setByteArray(const std::vector<std::uint8_t> &bytes) {
    call("__ZN10ByteStream12setByteArrayEPci", {stream_, library::copyBytes(bytes), bytes.size()});
    call("__ZN10ByteStream11resetOffsetEv", {stream_});
}
} // namespace server
