#include "logic/message/home/StartMissionMessage.h"

#include "library/LibraryMessage.h"

namespace server {
namespace {
// StartMissionMessage: LogicNpcData* +0x48 (a data table row: not owned).
constexpr int kNpcDataOffset = 0x48;
} // namespace

void StartMissionMessage::decode() {
    const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
    npcData_ = message.readPointer(kNpcDataOffset);
}
} // namespace server
