#pragma once
// Whether a name may be taken (the name popup checks as the player types). Payload: String name.
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class AvatarNameCheckRequestMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14600;

    void decode() override { name_ = stream_.readString(); }
    int getMessageType() const override { return kMessageType; }

    const std::optional<std::string> &getName() const { return name_; }

private:
    std::optional<std::string> name_;
};
} // namespace server
