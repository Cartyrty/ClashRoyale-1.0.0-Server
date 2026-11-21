#pragma once
// Whether a name may be taken (the name popup checks as the player types). Decoded by the library's own
// AvatarNameCheckRequestMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class AvatarNameCheckRequestMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 14600;

    void decode() override {
        // AvatarNameCheckRequestMessage: name String* +0x48.
        constexpr int kNameOffset = 0x48;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        name_ = message.readString(kNameOffset);
    }
    int getMessageType() const override { return kMessageType; }

    const std::optional<std::string> &getName() const { return name_; }

private:
    std::optional<std::string> name_;
};
} // namespace server
