#pragma once
// A new avatar name. Decoded by the library's own ChangeAvatarNameMessage.
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class ChangeAvatarNameMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 10212;

    void decode() override {
        // ChangeAvatarNameMessage: name String* +0x48 (at most 60), name set by user bool +0x50.
        constexpr int kNameOffset = 0x48;
        constexpr int kNameSetByUserOffset = 0x50;
        const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
        name_ = message.readString(kNameOffset);
        nameSetByUser_ = message.readBoolean(kNameSetByUserOffset);
    }
    int getMessageType() const override { return kMessageType; }

    const std::optional<std::string> &getName() const { return name_; }
    bool getNameSetByUser() const { return nameSetByUser_; }

private:
    std::optional<std::string> name_;
    bool nameSetByUser_ = false;
};
} // namespace server
