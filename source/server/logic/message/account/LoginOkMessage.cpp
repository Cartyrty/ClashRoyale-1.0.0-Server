#include "logic/message/account/LoginOkMessage.h"

#include "game/utils/TimeUtil.h"
#include "library/LibraryMessage.h"

namespace server {
namespace {
// LoginOkMessage (0xb8 bytes): account id and home id LogicLong* +0x48/+0x50, pass token +0x58, Facebook id +0x60,
// Game Center id +0x68, Facebook app id +0x70, server major / build / minor / content version +0x78..+0x84,
// environment +0x88, session count +0x90, play time +0x94, days since started playing +0x98, server time +0xa0,
// account creation date +0xa8 (both String milliseconds).
constexpr int kAccountIdOffset = 0x48;
constexpr int kHomeIdOffset = 0x50;
constexpr int kPassTokenOffset = 0x58;
constexpr int kFacebookAppIdOffset = 0x70;
constexpr int kServerMajorVersionOffset = 0x78;
constexpr int kServerBuildOffset = 0x7c;
constexpr int kServerMinorVersionOffset = 0x80;
constexpr int kContentVersionOffset = 0x84;
constexpr int kEnvironmentOffset = 0x88;
constexpr int kSessionCountOffset = 0x90;
constexpr int kPlayTimeOffset = 0x94;
constexpr int kDaysSinceStartedOffset = 0x98;
constexpr int kServerTimeOffset = 0xa0;
constexpr int kAccountCreatedDateOffset = 0xa8;

constexpr const char *kEnvironment = "prod";
constexpr const char *kFacebookAppId = "1475268786112433";
} // namespace

void LoginOkMessage::encode() {
    auto message = library::LibraryMessage::create(kMessageType);
    message.writePointer(kAccountIdOffset, library::newLogicLong(accountId_));
    message.writePointer(kHomeIdOffset, library::newLogicLong(accountId_));
    message.writePointer(kPassTokenOffset, library::newString(passToken_));
    message.writeInt(kServerMajorVersionOffset, kServerMajorVersion);
    message.writeInt(kServerBuildOffset, kServerBuild);
    message.writeInt(kServerMinorVersionOffset, kServerBuild);
    message.writeInt(kContentVersionOffset, kContentVersion);
    message.writePointer(kEnvironmentOffset, library::newString(kEnvironment));
    message.writeInt(kSessionCountOffset, sessionCount_);
    message.writeInt(kPlayTimeOffset, playTimeSeconds_);
    message.writeInt(kDaysSinceStartedOffset, daysSinceStartedPlaying_);
    message.writePointer(kFacebookAppIdOffset, library::newString(kFacebookAppId));
    message.writePointer(kServerTimeOffset, library::newString(std::to_string(TimeUtil::getMilliseconds())));
    message.writePointer(kAccountCreatedDateOffset, library::newString(std::to_string(accountCreatedDate_)));
    stream_.writeBytesWithoutLength(message.encode());
}
} // namespace server
