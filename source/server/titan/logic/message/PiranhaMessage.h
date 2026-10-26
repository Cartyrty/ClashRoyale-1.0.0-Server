#pragma once
// The base of every message, as on the client: a type, a version, and a ByteStream the message encodes itself into
// (server messages) or decodes itself from (client messages).
#include "titan/logic/datastream/ByteStream.h"

namespace server {
class PiranhaMessage {
public:
    virtual ~PiranhaMessage() = default;

    virtual void encode() {}
    virtual void decode() {}
    virtual int getMessageType() const = 0;
    virtual int getMessageVersion() const { return 0; }

    // Messaging encodes a message once, however many connections it goes to.
    void encodeOnce() {
        if (encoded_) return;
        encode();
        encoded_ = true;
    }

    ByteStream &getByteStream() { return stream_; }

protected:
    ByteStream stream_;

private:
    bool encoded_ = false;
};
} // namespace server
