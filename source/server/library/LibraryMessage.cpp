#include "library/LibraryMessage.h"

#include "titan/logic/debug/Debugger.h"

namespace library {
namespace {
constexpr int kMessageStreamOffset = 0x10;
constexpr int kEncodeSlot = 0x10;
constexpr int kDecodeSlot = 0x18;

Ptr messageFactory() {
    static const Ptr factory = [] {
        const auto created = allocate(0x10);
        call("__ZN25LogicScrollMessageFactoryC1Eb", {created, 0});
        return created;
    }();
    return factory;
}

Ptr streamOf(Ptr message) { return message + kMessageStreamOffset; }

std::vector<std::uint8_t> streamBytes(Ptr stream) {
    const int length = int(call("__ZNK10ByteStream9getLengthEv", {stream}));
    const auto data = call("__ZNK10ByteStream12getByteArrayEv", {stream});
    if (length <= 0 || !data) return {};
    return readBytes(data, std::size_t(length));
}
} // namespace

LibraryMessage LibraryMessage::create(int messageType) {
    const auto message =
        call("__ZN25LogicScrollMessageFactory19createMessageByTypeEi", {messageFactory(), std::uint64_t(messageType)});
    if (!message) server::Debugger::error("the library has no message of type %d", messageType);
    return LibraryMessage(message);
}

LibraryMessage LibraryMessage::decode(int messageType, const std::vector<std::uint8_t> &payload) {
    auto message = create(messageType);
    const auto stream = streamOf(message.message_);
    call("__ZN10ByteStream12setByteArrayEPci", {stream, copyBytes(payload), payload.size()});
    call("__ZN10ByteStream11resetOffsetEv", {stream});
    virtualCall(message.message_, kDecodeSlot);
    return message;
}

LibraryMessage::LibraryMessage(LibraryMessage &&other) noexcept : message_(other.message_) { other.message_ = 0; }

LibraryMessage &LibraryMessage::operator=(LibraryMessage &&other) noexcept {
    if (this != &other) {
        deleteObject(message_);
        message_ = other.message_;
        other.message_ = 0;
    }
    return *this;
}

LibraryMessage::~LibraryMessage() { deleteObject(message_); }

std::vector<std::uint8_t> LibraryMessage::encode() {
    virtualCall(message_, kEncodeSlot);
    return streamBytes(streamOf(message_));
}

std::int32_t LibraryMessage::readInt(int offset) const { return library::readInt(message_ + std::uint64_t(offset)); }
void LibraryMessage::writeInt(int offset, std::int32_t value) {
    library::writeInt(message_ + std::uint64_t(offset), value);
}
Ptr LibraryMessage::readPointer(int offset) const { return library::readPointer(message_ + std::uint64_t(offset)); }
void LibraryMessage::writePointer(int offset, Ptr value) {
    library::writePointer(message_ + std::uint64_t(offset), value);
}

Ptr LibraryMessage::takePointer(int offset) {
    const auto value = readPointer(offset);
    writePointer(offset, 0);
    return value;
}
} // namespace library
