#pragma once
// Whether the checked name may be taken. Encoded by the library's own AvatarNameCheckResponseMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class AvatarNameCheckResponseMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 20300;

    void encode() override {
        // AvatarNameCheckResponseMessage: invalid bool +0x44, error code +0x48, name String* +0x50.
        constexpr int kInvalidOffset = 0x44;
        constexpr int kErrorCodeOffset = 0x48;
        constexpr int kNameOffset = 0x50;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeBoolean(kInvalidOffset, invalid_);
        message.writeInt(kErrorCodeOffset, errorCode_);
        message.writeString(kNameOffset, name_);
        stream_.writeBytesWithoutLength(message.encode());
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
