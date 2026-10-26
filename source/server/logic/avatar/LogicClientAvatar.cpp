#include "logic/avatar/LogicClientAvatar.h"

#include "titan/logic/datastream/ByteStream.h"

namespace server {
using library::call;
using library::Ptr;

namespace {
// LogicClientAvatar (0xf8 bytes): ids inline at +0x10 (avatar), +0x18 (account), +0x20 (home); name String +0x28;
// score +0x40; exp level +0x78; alliance badge data +0xa0, alliance name String +0xb0.
constexpr std::size_t kSize = 0xf8;
constexpr int kIdOffsets[] = {0x10, 0x18, 0x20};
constexpr int kAccountIdOffset = 0x18;
constexpr int kNameOffset = 0x28;
constexpr int kScoreOffset = 0x40;
constexpr int kExpLevelOffset = 0x78;
constexpr int kAllianceBadgeOffset = 0xa0;
constexpr int kAllianceNameOffset = 0xb0;
// Virtual slots: encode(ChecksumEncoder*), decode(ByteStream*).
constexpr int kEncodeSlot = 0x18;
constexpr int kDecodeSlot = 0x20;

std::uint64_t argument(int value) { return std::uint64_t(std::uint32_t(value)); }
} // namespace

LogicClientAvatar LogicClientAvatar::create() {
    const auto avatar = library::allocate(kSize);
    call("__ZN17LogicClientAvatarC1Ev", {avatar});
    return LogicClientAvatar(avatar, true);
}

LogicClientAvatar LogicClientAvatar::decode(const std::vector<std::uint8_t> &encoded) {
    auto avatar = create();
    ByteStream stream(encoded);
    library::virtualCall(avatar.avatar_, kDecodeSlot, {stream.getLibraryStream()});
    return avatar;
}

LogicClientAvatar LogicClientAvatar::view(Ptr avatar) { return LogicClientAvatar(avatar, false); }

LogicClientAvatar::LogicClientAvatar(LogicClientAvatar &&other) noexcept
    : avatar_(other.avatar_), owned_(other.owned_) {
    other.avatar_ = 0;
    other.owned_ = false;
}

LogicClientAvatar &LogicClientAvatar::operator=(LogicClientAvatar &&other) noexcept {
    if (this != &other) {
        destroy();
        avatar_ = other.avatar_;
        owned_ = other.owned_;
        other.avatar_ = 0;
        other.owned_ = false;
    }
    return *this;
}

LogicClientAvatar::~LogicClientAvatar() { destroy(); }

void LogicClientAvatar::destroy() {
    if (owned_) library::deleteObject(avatar_);
    avatar_ = 0;
}

Ptr LogicClientAvatar::release() {
    const auto avatar = avatar_;
    avatar_ = 0;
    owned_ = false;
    return avatar;
}

std::vector<std::uint8_t> LogicClientAvatar::encode() const {
    ByteStream stream;
    encode(stream);
    return stream.getByteArray();
}

void LogicClientAvatar::encode(ByteStream &stream) const {
    library::virtualCall(avatar_, kEncodeSlot, {stream.getLibraryStream()});
}

void LogicClientAvatar::setIds(std::int64_t id) {
    for (int offset : kIdOffsets) library::writeLogicLong(avatar_ + offset, id);
}

std::int64_t LogicClientAvatar::getAccountId() const { return library::readLogicLong(avatar_ + kAccountIdOffset); }
std::string LogicClientAvatar::getName() const { return library::readString(avatar_ + kNameOffset); }
void LogicClientAvatar::setName(const std::string &name) {
    call("__ZN17LogicClientAvatar7setNameERK6String", {avatar_, library::newString(name)});
}
bool LogicClientAvatar::getNameSetByUser() const {
    return (call("__ZNK17LogicClientAvatar16getNameSetByUserEv", {avatar_}) & 0xff) != 0;
}
int LogicClientAvatar::getNameChangeState() const {
    return int(call("__ZNK17LogicClientAvatar18getNameChangeStateEv", {avatar_}));
}
int LogicClientAvatar::getExpLevel() const { return library::readInt(avatar_ + kExpLevelOffset); }
void LogicClientAvatar::setExpLevel(int level) {
    call("__ZN17LogicClientAvatar11setExpLevelEi", {avatar_, argument(level)});
}
int LogicClientAvatar::getExpPoints() const { return int(call("__ZNK17LogicClientAvatar12getExpPointsEv", {avatar_})); }
void LogicClientAvatar::setExpPoints(int points) {
    call("__ZN17LogicClientAvatar12setExpPointsEi", {avatar_, argument(points)});
}
int LogicClientAvatar::getScore() const { return library::readInt(avatar_ + kScoreOffset); }
void LogicClientAvatar::setScore(int score) { library::writeInt(avatar_ + kScoreOffset, score); }
int LogicClientAvatar::getGold() const { return int(call("__ZNK17LogicClientAvatar7getGoldEv", {avatar_})); }

void LogicClientAvatar::setCommodityCount(int commodityType, Ptr data, int count) {
    call("__ZN17LogicClientAvatar17setCommodityCountEiPK9LogicDatai",
         {avatar_, argument(commodityType), data, argument(count)});
}

void LogicClientAvatar::setDiamonds(int diamonds) {
    call("__ZN17LogicClientAvatar11setDiamondsEi", {avatar_, argument(diamonds)});
}
void LogicClientAvatar::setFreeDiamonds(int diamonds) {
    call("__ZN17LogicClientAvatar15setFreeDiamondsEi", {avatar_, argument(diamonds)});
}
Ptr LogicClientAvatar::getArena() const { return call("__ZNK17LogicClientAvatar8getArenaEv", {avatar_}); }
void LogicClientAvatar::setArena(Ptr arenaData) {
    call("__ZN17LogicClientAvatar8setArenaEPK14LogicArenaData", {avatar_, arenaData});
}
std::string LogicClientAvatar::getAllianceName() const { return library::readString(avatar_ + kAllianceNameOffset); }
Ptr LogicClientAvatar::getAllianceBadge() const { return library::readPointer(avatar_ + kAllianceBadgeOffset); }

Ptr LogicClientAvatar::getNextWinnerChest(Ptr arenaData) const {
    return call("__ZNK17LogicClientAvatar18getNextWinnerChestEPK14LogicArenaData", {avatar_, arenaData});
}

void LogicClientAvatar::updateWinnerChestIndex() {
    call("__ZN17LogicClientAvatar22updateWinnerChestIndexEv", {avatar_});
}
} // namespace server
