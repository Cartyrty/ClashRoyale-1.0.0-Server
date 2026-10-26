#pragma once
// Clan mail to every member.
#include "logic/command/LogicCommandReader.h"

#include <optional>
#include <string>

namespace server {
class LogicSendAllianceMailCommand {
public:
    static constexpr int kCommandType = 533;

    explicit LogicSendAllianceMailCommand(const LogicCommand &command) {
        auto stream = readCommandFields(command);
        message_ = stream.readString();
        title_ = stream.readString();
    }
    const std::optional<std::string> &getMessage() const { return message_; }
    const std::optional<std::string> &getTitle() const { return title_; }

private:
    std::optional<std::string> message_;
    std::optional<std::string> title_;
};
} // namespace server
