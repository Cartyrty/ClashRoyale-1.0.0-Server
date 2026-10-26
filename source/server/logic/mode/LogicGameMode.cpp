#include "logic/mode/LogicGameMode.h"

#include "titan/logic/datastream/ByteStream.h"

namespace server {
using library::call;
using library::Ptr;

namespace {
// LogicGameMode (0x1b0 bytes): LogicCommandManager* +0x38, LogicBattle* +0xa8.
constexpr std::size_t kSize = 0x1b0;
constexpr int kCommandManagerOffset = 0x38;
constexpr int kBattleOffset = 0xa8;
// loadHomeState's "time since the home was saved on the client" (not used by the server).
constexpr int kNoClientTime = -1;

std::uint64_t argument(int value) { return std::uint64_t(std::uint32_t(value)); }
} // namespace

LogicGameMode::LogicGameMode(bool battle) : mode_(library::allocate(kSize)) {
    call("__ZN13LogicGameModeC1Eb", {mode_, battle ? 1u : 0u});
}

LogicGameMode::LogicGameMode(LogicGameMode &&other) noexcept : mode_(other.mode_) { other.mode_ = 0; }

LogicGameMode &LogicGameMode::operator=(LogicGameMode &&other) noexcept {
    if (this != &other) {
        destroy();
        mode_ = other.mode_;
        other.mode_ = 0;
    }
    return *this;
}

LogicGameMode::~LogicGameMode() { destroy(); }

void LogicGameMode::destroy() {
    if (!mode_) return;
    call("__ZN13LogicGameMode8destructEv", {mode_});
    library::release(mode_);
    mode_ = 0;
}

void LogicGameMode::loadHomeState(LogicClientHome home, LogicClientAvatar avatar, int secondsSinceSave,
                                  int randomSeed) {
    call("__ZN13LogicGameMode13loadHomeStateEP15LogicClientHomeP17LogicClientAvatariii",
         {mode_, home.release(), avatar.release(), argument(secondsSinceSave), argument(kNoClientTime),
          argument(randomSeed)});
}

LogicClientHome LogicGameMode::getHome() const {
    return LogicClientHome::view(call("__ZNK13LogicGameMode7getHomeEv", {mode_}));
}

LogicClientAvatar LogicGameMode::getPlayerAvatar() const {
    return LogicClientAvatar::view(call("__ZNK13LogicGameMode15getPlayerAvatarEv", {mode_}));
}

int LogicGameMode::getTick() const { return int(call("__ZNK13LogicGameMode7getTickEv", {mode_})); }
void LogicGameMode::updateOneTick() { call("__ZN13LogicGameMode13updateOneTickEv", {mode_}); }
int LogicGameMode::calculateChecksum() const { return int(call("__ZNK13LogicGameMode17calculateChecksumEv", {mode_})); }

void LogicGameMode::addCommand(LogicCommand command) {
    call("__ZN19LogicCommandManager10addCommandEP12LogicCommand",
         {library::readPointer(mode_ + kCommandManagerOffset), command.release()});
}

LogicBattle LogicGameMode::getBattle() const { return LogicBattle(library::readPointer(mode_ + kBattleOffset)); }

void LogicGameMode::setLogicChangesAllowed(bool allowed) {
    call("__ZN13LogicGameMode22setLogicChangesAllowedEb", {mode_, allowed ? 1u : 0u});
}

void LogicGameMode::setRandomSeed(int seed) { call("__ZN13LogicGameMode13setRandomSeedEi", {mode_, argument(seed)}); }

void LogicGameMode::addPlayer(LogicClientAvatar avatar) {
    call("__ZN13LogicGameMode9addPlayerEP17LogicClientAvatar", {mode_, avatar.release()});
}

Ptr LogicGameMode::getAvatarByAccountId(std::int64_t accountId) const {
    return call("__ZNK13LogicGameMode20getAvatarByAccountIDEii",
                {mode_, argument(int(accountId >> 32)), argument(int(accountId))});
}

void LogicGameMode::encode(ByteStream &stream, Ptr viewerAvatar) const {
    call("__ZNK13LogicGameMode6encodeEP15ChecksumEncoderbPK17LogicClientAvatar",
         {mode_, stream.getLibraryStream(), 1, viewerAvatar});
}

void LogicGameMode::decode(ByteStream &stream) {
    call("__ZN13LogicGameMode6decodeEP10ByteStreambP17LogicGameListener", {mode_, stream.getLibraryStream(), 1, 0});
}
} // namespace server
