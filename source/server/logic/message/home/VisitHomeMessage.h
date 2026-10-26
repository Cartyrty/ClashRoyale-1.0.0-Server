#pragma once
// Another player's profile. Payload: Long homeId.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>

namespace server {
class VisitHomeMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14113;

    void decode() override { homeId_ = stream_.readLong(); }
    int getMessageType() const override { return kMessageType; }

    std::int64_t getHomeId() const { return homeId_; }

private:
    std::int64_t homeId_ = 0;
};
} // namespace server
