#pragma once
// A new avatar name (ChangeAvatarNameMessage accepted), with the name change state it leaves.
#include "logic/command/server/LogicServerCommand.h"

#include <string>

namespace server {
class LogicChangeAvatarNameCommand : public LogicServerCommand {
public:
    static constexpr int kCommandType = 201;

    LogicChangeAvatarNameCommand(std::string name, int nameChangeState, bool setByUser)
        : name_(std::move(name)), nameChangeState_(nameChangeState), setByUser_(setByUser) {}
    int getCommandType() const override { return kCommandType; }

protected:
    void encodeFields(ByteStream &stream) const override {
        stream.writeString(name_);
        stream.writeInt(nameChangeState_);
        stream.writeBoolean(setByUser_);
    }

private:
    std::string name_;
    int nameChangeState_;
    bool setByUser_;
};
} // namespace server
