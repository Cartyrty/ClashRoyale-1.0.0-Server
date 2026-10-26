#include "logic/command/LogicCommand.h"

#include "logic/mode/LogicGameMode.h"
#include "titan/logic/datastream/ByteStream.h"
#include "titan/logic/datastream/ByteStreamHelper.h"
#include "titan/logic/json/LogicJSON.h"

namespace server {
using library::call;

namespace {
constexpr int kExecuteSlot = 0x18;
constexpr int kCommandTypeSlot = 0x20;
} // namespace

LogicCommand LogicCommand::decode(const std::vector<std::uint8_t> &encoded) {
    ByteStream stream(encoded);
    return LogicCommand(
        call("__ZN19LogicCommandManager13decodeCommandEP10ByteStreambb", {stream.getLibraryStream(), 1, 1}));
}

std::vector<std::uint8_t> LogicCommand::encode() const {
    ByteStream stream;
    call("__ZN19LogicCommandManager13encodeCommandEP15ChecksumEncoderPK12LogicCommand",
         {stream.getLibraryStream(), get()});
    return stream.getByteArray();
}

std::string LogicCommand::saveToJSON() const {
    const auto object = LogicJSON::createObject();
    call("__ZN19LogicCommandManager17saveCommandToJSONEP15LogicJSONObjectPK12LogicCommand", {object, get()});
    return LogicJSON::writeObject(object);
}

void LogicCommand::skipBaseFields(ByteStream &stream) {
    stream.readVInt();
    stream.readVInt();
    ByteStreamHelper::decodeLogicLong(stream);
}

void LogicCommand::execute(LogicGameMode &mode) { library::virtualCall(get(), kExecuteSlot, {mode.get(), 0, 0}); }

int LogicCommand::getCommandType() const { return int(library::virtualCall(get(), kCommandTypeSlot)); }

bool LogicCommand::isServerCommand() const {
    const int type = getCommandType();
    return type >= kFirstServerCommandType && type <= kLastServerCommandType;
}

int LogicCommand::getExecuteTick() const { return int(call("__ZNK12LogicCommand14getExecuteTickEv", {get()})); }

void LogicCommand::setExecuteTick(int tick) {
    call("__ZN12LogicCommand14setExecuteTickEi", {get(), std::uint64_t(std::uint32_t(tick))});
}

int LogicCommand::getTickWhenGiven() const { return int(call("__ZNK12LogicCommand16getTickWhenGivenEv", {get()})); }

std::int64_t LogicCommand::getExecutorAccountId() const {
    return library::readLogicLong(call("__ZNK12LogicCommand20getExecutorAccountIdEv", {get()}));
}

void LogicCommand::setExecutorAccountId(std::int64_t accountId) {
    call("__ZN12LogicCommand20setExecutorAccountIdERK9LogicLong", {get(), library::newLogicLong(accountId)});
}
} // namespace server
