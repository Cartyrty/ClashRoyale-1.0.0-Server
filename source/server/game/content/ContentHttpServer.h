#pragma once
// Content hosting, as content.js does it: a small HTTP server on the content port serving content/update (GET
// /<sha>/<file>), driven by the server's select loop.
#include "platform/Socket.h"

#include <cstdint>
#include <list>
#include <string>

namespace server {
class ContentHttpServer {
public:
    ContentHttpServer(const std::string &resourceDirectory, const std::string &dataDirectory,
                      const std::string &currentSha);
    ~ContentHttpServer();
    ContentHttpServer(const ContentHttpServer &) = delete;
    ContentHttpServer &operator=(const ContentHttpServer &) = delete;

    // False (content updates off) when the port cannot be used.
    bool listen(const std::string &host, int port);
    void addSockets(fd_set &readable, fd_set &writable) const;
    void service(const fd_set &readable, const fd_set &writable);
    platform::socket_t getHighestSocket() const; // for select()'s nfds

private:
    struct Client {
        std::uintptr_t socket;
        std::string peer;
        std::string request;
        std::string response;
        std::size_t sent = 0;
        std::int64_t startTime = 0;
    };
    bool isListening() const { return listener_ != kNoSocket; }
    void respond(Client &client);

    static constexpr std::uintptr_t kNoSocket = ~std::uintptr_t(0);

    std::string resourceDirectory_;
    std::string updateDirectory_;
    std::string currentSha_;
    std::uintptr_t listener_ = kNoSocket;
    std::list<Client> clients_;
};
} // namespace server
