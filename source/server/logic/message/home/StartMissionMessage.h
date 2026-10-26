#pragma once
// An NPC battle (training camp, missions): the NPC (npcs.csv). Decoded by the library's own StartMissionMessage.
#include "library/Library.h"
#include "titan/logic/message/PiranhaMessage.h"

namespace server {
class StartMissionMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14104;

    void decode() override;
    int getMessageType() const override { return kMessageType; }

    library::Ptr getNpcData() const { return npcData_; } // LogicNpcData*, 0 when none

private:
    library::Ptr npcData_ = 0;
};
} // namespace server
