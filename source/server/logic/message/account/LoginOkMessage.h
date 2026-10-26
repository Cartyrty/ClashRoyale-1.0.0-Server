#pragma once
// The login went through: the account's ids and pass token (the client keeps them), its statistics, and the server's
// version and time. Encoded by the library's own LoginOkMessage.
#include "titan/logic/message/PiranhaMessage.h"

#include <cstdint>
#include <string>

namespace server {
class LoginOkMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 20104;
    // The version the 1.0.0 client expects (fingerprint 2.1306.1).
    static constexpr int kServerMajorVersion = 2;
    static constexpr int kServerBuild = 1306;
    static constexpr int kContentVersion = 1;

    void encode() override;
    int getMessageType() const override { return kMessageType; }
    int getMessageVersion() const override { return 1; }

    void setAccountId(std::int64_t accountId) { accountId_ = accountId; }
    void setPassToken(const std::string &passToken) { passToken_ = passToken; }
    void setSessionCount(int sessionCount) { sessionCount_ = sessionCount; }
    void setPlayTimeSeconds(int playTimeSeconds) { playTimeSeconds_ = playTimeSeconds; }
    void setDaysSinceStartedPlaying(int days) { daysSinceStartedPlaying_ = days; }
    void setAccountCreatedDate(std::int64_t milliseconds) { accountCreatedDate_ = milliseconds; }

private:
    std::int64_t accountId_ = 0;
    std::string passToken_;
    int sessionCount_ = 0;
    int playTimeSeconds_ = 0;
    int daysSinceStartedPlaying_ = 0;
    std::int64_t accountCreatedDate_ = 0;
};
} // namespace server
