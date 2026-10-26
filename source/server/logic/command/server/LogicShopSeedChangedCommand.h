#pragma once
// A new daily shop: the client rolls the same cards from the seed (LogicClientHome::setShopValues).
#include "logic/command/server/LogicServerCommand.h"

namespace server {
class LogicShopSeedChangedCommand : public LogicServerCommand {
public:
    static constexpr int kCommandType = 217;

    LogicShopSeedChangedCommand(int seed, int secondsLeft, int weekday)
        : seed_(seed), secondsLeft_(secondsLeft), weekday_(weekday) {}
    int getCommandType() const override { return kCommandType; }

protected:
    void encodeFields(ByteStream &stream) const override {
        stream.writeVInt(seed_);
        stream.writeVInt(secondsLeft_);
        stream.writeVInt(weekday_);
    }

private:
    int seed_;
    int secondsLeft_;
    int weekday_;
};
} // namespace server
