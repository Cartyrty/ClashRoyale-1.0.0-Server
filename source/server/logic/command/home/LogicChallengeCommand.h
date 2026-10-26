#pragma once
// A friendly battle challenge to the alliance, with an optional message.
#include "logic/command/LogicCommandReader.h"

#include <optional>
#include <string>

namespace server {
class LogicChallengeCommand {
public:
    static constexpr int kCommandType = 534;

    explicit LogicChallengeCommand(const LogicCommand &command) {
        auto stream = readCommandFields(command);
        if (stream.readBoolean()) message_ = stream.readString();
    }
    const std::optional<std::string> &getMessage() const { return message_; }

private:
    std::optional<std::string> message_;
};
} // namespace server
