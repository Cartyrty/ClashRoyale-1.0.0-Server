#pragma once
// A member kicked out, with an optional message for them.
#include "logic/command/LogicCommandReader.h"

#include <cstdint>
#include <optional>
#include <string>

namespace server {
class LogicKickAllianceMemberCommand {
public:
    static constexpr int kCommandType = 526;

    explicit LogicKickAllianceMemberCommand(const LogicCommand &command) {
        auto stream = readCommandFields(command);
        LogicCommand::skipBaseFields(stream);
        message_ = stream.readString();
        avatarId_ = stream.readLong();
    }
    const std::optional<std::string> &getMessage() const { return message_; }
    std::int64_t getAvatarId() const { return avatarId_; }

private:
    std::optional<std::string> message_;
    std::int64_t avatarId_ = 0;
};
} // namespace server
