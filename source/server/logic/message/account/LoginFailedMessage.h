#pragma once
// The login was refused. Error 7 (outdated content) makes GameMain start InitState::startContentUpdate with the
// fingerprint and the content URL: the client downloads the changed files and logs in again.
// Payload: VInt error code, String resource fingerprint data, String redirect domain, String content URL, then
// (version >= 1) String update URL, (>= 2) String reason, and VInt seconds until the maintenance ends. Encoded by the
// library's own LoginFailedMessage (version 2 from its constructor).
#include "library/LibraryMessage.h"
#include "titan/logic/message/PiranhaMessage.h"

#include <optional>
#include <string>

namespace server {
class LoginFailedMessage : public PiranhaMessage {
public:
    static constexpr int kMessageType = 20103;
    static constexpr int kErrorContentUpdate = 7;

    void encode() override {
        // LoginFailedMessage: error code +0x44, resource fingerprint data String* +0x48, redirect domain String*
        // +0x50, content URL String* +0x58, update URL String* +0x60, reason String* +0x68, seconds until the
        // maintenance ends +0x70.
        constexpr int kErrorCodeOffset = 0x44;
        constexpr int kResourceFingerprintDataOffset = 0x48;
        constexpr int kRedirectDomainOffset = 0x50;
        constexpr int kContentUrlOffset = 0x58;
        constexpr int kUpdateUrlOffset = 0x60;
        constexpr int kReasonOffset = 0x68;
        constexpr int kSecondsUntilMaintenanceEndOffset = 0x70;
        auto message = library::LibraryMessage::create(kMessageType);
        message.writeInt(kErrorCodeOffset, errorCode_);
        message.writeString(kResourceFingerprintDataOffset, resourceFingerprintData_);
        message.writeString(kRedirectDomainOffset, redirectDomain_);
        message.writeString(kContentUrlOffset, contentUrl_);
        message.writeString(kUpdateUrlOffset, updateUrl_);
        message.writeString(kReasonOffset, reason_);
        message.writeInt(kSecondsUntilMaintenanceEndOffset, secondsUntilMaintenanceEnd_);
        stream_.writeBytesWithoutLength(message.encode());
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
