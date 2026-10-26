#pragma once
// A card request to the alliance: the message and the requested cards.
#include "logic/command/LogicCommandReader.h"
#include "titan/logic/datastream/ByteStreamHelper.h"

#include <optional>
#include <string>
#include <vector>

namespace server {
class LogicRequestSpellsCommand {
public:
    static constexpr int kCommandType = 524;
    static constexpr int kMaxSpells = 200;

    explicit LogicRequestSpellsCommand(const LogicCommand &command) {
        auto stream = readCommandFields(command);
        LogicCommand::skipBaseFields(stream);
        message_ = stream.readString();
        const int count = stream.readVInt();
        for (int i = 0; i < count && i < kMaxSpells; ++i)
            spells_.push_back(ByteStreamHelper::readDataReference(stream));
    }
    const std::optional<std::string> &getMessage() const { return message_; }
    const std::vector<int> &getSpells() const { return spells_; } // global ids

private:
    std::optional<std::string> message_;
    std::vector<int> spells_;
};
} // namespace server
