#pragma once
// The login was refused. Error 7 (outdated content) makes GameMain start InitState::startContentUpdate with the
// fingerprint and the content URL: the client downloads the changed files and logs in again.
// Payload: VInt error code, String resource fingerprint data, String redirect domain, String content URL, then
// (version >= 1) String update URL, (>= 2) String reason, and VInt seconds until the maintenance ends.
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class LoginFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 20103;
    static constexpr int kErrorContentUpdate = 7;

    void encode() override {
        stream_.writeVInt(errorCode_);
        stream_.writeString(resourceFingerprintData_);
        stream_.writeString(redirectDomain_);
        stream_.writeString(contentUrl_);
        stream_.writeString(updateUrl_);
        stream_.writeString(reason_);
        stream_.writeVInt(secondsUntilMaintenanceEnd_);
    }
    int getMessageType() const override { return kMessageType; }
    int getMessageVersion() const override { return 2; }

    void setErrorCode(int errorCode) { errorCode_ = errorCode; }
    void setResourceFingerprintData(const std::string &fingerprintJson) { resourceFingerprintData_ = fingerprintJson; }
    void setContentUrl(const std::string &url) { contentUrl_ = url; }

private:
    int errorCode_ = 0;
    std::optional<std::string> resourceFingerprintData_;
    std::optional<std::string> redirectDomain_;
    std::optional<std::string> contentUrl_;
    std::optional<std::string> updateUrl_;
    std::optional<std::string> reason_;
    int secondsUntilMaintenanceEnd_ = 0;
};
} // namespace server
