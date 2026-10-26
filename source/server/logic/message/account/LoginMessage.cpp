#include "logic/message/account/LoginMessage.h"

#include "library/LibraryMessage.h"

namespace server {
namespace {
// LoginMessage: account id LogicLong* +0x48, pass token String* +0x50, resource SHA String* +0x68.
constexpr int kAccountIdOffset = 0x48;
constexpr int kPassTokenOffset = 0x50;
constexpr int kResourceShaOffset = 0x68;
} // namespace

void LoginMessage::decode() {
    const auto message = library::LibraryMessage::decode(kMessageType, stream_.getByteArray());
    accountId_ = library::readLogicLong(message.readPointer(kAccountIdOffset));
    passToken_ = library::readStringPointer(message.readPointer(kPassTokenOffset));
    resourceSha_ = library::readStringPointer(message.readPointer(kResourceShaOffset));
}
} // namespace server
