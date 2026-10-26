#include "logic/message/home/EndClientTurnMessage.h"

#include "library/LibraryMessage.h"

namespace server {
namespace {
// EndClientTurnMessage: LogicArrayList<LogicCommand*>* +0x48, tick +0x50, checksum +0x54.
constexpr int kCommandsOffset = 0x48;
constexpr int kTickOffset = 0x50;
constexpr int kChecksumOffset = 0x54;
} // namespace

void EndClientTurnMessage::decode() {
    auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
    tick_ = message.readInt(kTickOffset);
    checksum_ = message.readInt(kChecksumOffset);
    const auto list = message.readPointer(kCommandsOffset);
    for (int i = 0; i < library::arrayCount(list); ++i) {
        const auto command = library::arrayItem(list, i);
        library::setArrayItem(list, i, 0); // the message's destructor would free it
        if (command) commands_.emplace_back(command);
    }
}
} // namespace server
