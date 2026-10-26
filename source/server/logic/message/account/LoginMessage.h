#pragma once
// The login: the account (0 or unknown: a new one is made) and its pass token, and the master SHA of the client's
// fingerprint.json (content updates). Decoded by the library's own LoginMessage.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <string>

namespace server {
class LoginMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 10101;

    void decode() override;
    int getMessageType() const override { return kMessageType; }

    std::int64_t getAccountId() const { return accountId_; }
    const std::string &getPassToken() const { return passToken_; }
    const std::string &getResourceSha() const { return resourceSha_; }

private:
    std::int64_t accountId_ = 0;
    std::string passToken_;
    std::string resourceSha_;
};
} // namespace server
