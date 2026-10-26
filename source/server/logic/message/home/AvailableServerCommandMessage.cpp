#include "logic/message/home/AvailableServerCommandMessage.h"

#include "library/LibraryMessage.h"

namespace server {
namespace {
// AvailableServerCommandMessage: LogicServerCommand* +0x48 (owned by the message).
constexpr int kCommandOffset = 0x48;
} // namespace

void AvailableServerCommandMessage::encode() {
    auto message = library::LibraryMessage::create(kMessageType);
    message.writePointer(kCommandOffset, command_.release());
    stream_.writeBytesWithoutLength(message.encode());
}
} // namespace server
