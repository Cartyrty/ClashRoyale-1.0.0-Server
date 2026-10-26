#pragma once
// The player looks for a PvP opponent.
#include "logic/command/LogicCommandReader.h"

namespace server {
class LogicStartMatchmakeCommand {
public:
    static constexpr int kCommandType = 537;

    explicit LogicStartMatchmakeCommand(const LogicCommand &command) {
        auto stream = readCommandFields(command);
        LogicCommand::skipBaseFields(stream);
        stream.readVInt();
        useDiamonds_ = stream.readBoolean();
    }
    bool getUseDiamonds() const { return useDiamonds_; }

private:
    bool useDiamonds_ = false;
};
} // namespace server
