#pragma once
// Whether the checked name may be taken. Payload: Boolean invalid, Int errorCode, String name.
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class AvatarNameCheckResponseMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 20300;

    void encode() override {
        stream_.writeBoolean(invalid_);
        stream_.writeInt(errorCode_);
        stream_.writeString(name_);
    }
    int getMessageType() const override { return kMessageType; }

    void setInvalid(bool invalid) { invalid_ = invalid; }
    void setErrorCode(int errorCode) { errorCode_ = errorCode; } // see AvatarNameRules
    void setName(const std::optional<std::string> &name) { name_ = name; }

private:
    bool invalid_ = false;
    int errorCode_ = 0;
    std::optional<std::string> name_;
};
} // namespace server
