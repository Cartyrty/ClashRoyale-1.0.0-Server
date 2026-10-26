#pragma once
// A command of the library (LogicCommand and its subclasses), owned: deleted unless handed to a command manager.
// Encoded and decoded the way LogicCommandManager does it (VInt type, then the command's own encode()).
#include "library/Library.h"

#include <cstdint>
#include <string>
#include <vector>

namespace server {
class ByteStream;
class LogicGameMode;

class LogicCommand {
public:
    // Command types are LogicCommand::getCommandType values; 200-499 are server commands (LogicServerCommand).
    static constexpr int kFirstServerCommandType = 200;
    static constexpr int kLastServerCommandType = 499;

    LogicCommand() = default;
    explicit LogicCommand(library::Ptr command) : command_(command) {}

    // LogicCommandManager::decodeCommand; an empty LogicCommand when the encoding is not a command.
    static LogicCommand decode(const std::vector<std::uint8_t> &encoded);
    std::vector<std::uint8_t> encode() const; // LogicCommandManager::encodeCommand
    std::string saveToJSON() const;           // LogicCommandManager::saveCommandToJSON, for replays
    // LogicCommand::decode's fields (VInt tick when given, VInt execute tick, LogicLong executor), skipped by the
    // readers of client commands.
    static void skipBaseFields(ByteStream &stream);

    // LogicCommand::execute(LogicGameMode*, int, bool) outside a command manager (a stored account's home).
    void execute(LogicGameMode &mode);

    int getCommandType() const;
    bool isServerCommand() const;
    int getExecuteTick() const;
    void setExecuteTick(int tick);
    int getTickWhenGiven() const;
    std::int64_t getExecutorAccountId() const;
    void setExecutorAccountId(std::int64_t accountId);

    library::Ptr get() const { return command_.get(); }
    library::Ptr release() { return command_.release(); }
    explicit operator bool() const { return bool(command_); }

private:
    library::Object command_;
};
} // namespace server
