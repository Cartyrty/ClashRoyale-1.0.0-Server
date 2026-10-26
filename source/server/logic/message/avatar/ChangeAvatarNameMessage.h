#pragma once
// A new avatar name. Payload: String(60) name, Boolean nameSetByUser.
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class ChangeAvatarNameMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 10212;

    void decode() override {
        name_ = stream_.readString(60);
        nameSetByUser_ = stream_.readBoolean();
    }
    int getMessageType() const override { return kMessageType; }

    const std::optional<std::string> &getName() const { return name_; }
    bool getNameSetByUser() const { return nameSetByUser_; }

private:
    std::optional<std::string> name_;
    bool nameSetByUser_ = false;
};
} // namespace server
