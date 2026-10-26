#include "game/content/ContentHttpServer.h"

#include "game/utils/FileUtil.h"
#include "game/utils/TimeUtil.h"
#include "titan/logic/debug/Debugger.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace server {
namespace fs = std::filesystem;

namespace {
constexpr std::int64_t kClientTimeoutMilliseconds = 60000;
constexpr std::size_t kMaxClients = 32;
constexpr std::size_t kMaxRequestSize = 16384;
constexpr std::size_t kSendChunkSize = 1 << 16;
constexpr int kListenBacklog = 16;

std::string decodeUrl(const std::string &text) {
    std::string decoded;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size()) {
            decoded += char(std::strtol(text.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else {
            decoded += text[i];
        }
    }
    return decoded;
}
} // namespace

ContentHttpServer::ContentHttpServer(const std::string &resourceDirectory, const std::string &dataDirectory,
                                     const std::string &currentSha)
    : resourceDirectory_(resourceDirectory),
      updateDirectory_((fs::path(dataDirectory) / "content" / "update").string()),
      currentSha_(currentSha) {}

ContentHttpServer::~ContentHttpServer() {
    for (auto &client : clients_) platform::closeSocket(platform::socket_t(client.socket));
    if (isListening()) platform::closeSocket(platform::socket_t(listener_));
}

bool ContentHttpServer::listen(const std::string &host, int port) {
    const platform::socket_t listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    platform::prepareListener(listener);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(std::uint16_t(port));
    inet_pton(AF_INET, host.c_str(), &address.sin_addr);
    if (bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof address) != 0 ||
        ::listen(listener, kListenBacklog) != 0) {
        Debugger::warning("content: cannot listen on %s:%d (error %d); content updates are off", host.c_str(), port,
                          platform::lastNetErrorCode());
        platform::closeSocket(listener);
        return false;
    }
    platform::setNonBlocking(listener, true);
    listener_ = std::uintptr_t(listener);
    Debugger::print("content", "content server listening on %s:%d (%s)", host.c_str(), port, updateDirectory_.c_str());
    return true;
}

platform::socket_t ContentHttpServer::getHighestSocket() const {
    platform::socket_t highest = 0;
    if (isListening()) highest = platform::socket_t(listener_);
    for (const auto &client : clients_) highest = std::max(highest, platform::socket_t(client.socket));
    return highest;
}

void ContentHttpServer::addSockets(fd_set &readable, fd_set &writable) const {
    if (!isListening()) return;
    FD_SET(platform::socket_t(listener_), &readable);
    for (const auto &client : clients_) {
        if (client.response.empty())
            FD_SET(platform::socket_t(client.socket), &readable);
        else
            FD_SET(platform::socket_t(client.socket), &writable);
    }
}

void ContentHttpServer::service(const fd_set &readable, const fd_set &writable) {
    if (!isListening()) return;
    if (FD_ISSET(platform::socket_t(listener_), &readable)) {
        for (;;) {
            sockaddr_in from{};
            platform::socklen length = sizeof from;
            const platform::socket_t accepted =
                accept(platform::socket_t(listener_), reinterpret_cast<sockaddr *>(&from), &length);
            if (accepted == platform::kInvalidSocket) break;
            if (clients_.size() >= kMaxClients) {
                platform::closeSocket(accepted);
                continue;
            }
            platform::setNonBlocking(accepted, true);
            char text[64];
            inet_ntop(AF_INET, &from.sin_addr, text, sizeof text);
            clients_.push_back({std::uintptr_t(accepted), text, {}, {}, 0, TimeUtil::getMilliseconds()});
        }
    }
    char buffer[4096];
    for (auto client = clients_.begin(); client != clients_.end();) {
        const auto socket = platform::socket_t(client->socket);
        bool done = false;
        if (client->response.empty() && FD_ISSET(socket, &readable)) {
            const int received = int(recv(socket, buffer, sizeof buffer, 0));
            if (received <= 0) {
                done = true;
            } else {
                client->request.append(buffer, std::size_t(received));
                if (client->request.find("\r\n\r\n") != std::string::npos)
                    respond(*client);
                else if (client->request.size() > kMaxRequestSize)
                    done = true;
            }
        } else if (!client->response.empty() && FD_ISSET(socket, &writable)) {
            const int sent =
                int(send(socket, client->response.data() + client->sent,
                         int(std::min(client->response.size() - client->sent, kSendChunkSize)), platform::kNoSignal));
            if (sent > 0)
                client->sent += std::size_t(sent);
            else if (platform::lastNetError() != platform::NetError::WouldBlock)
                done = true;
            if (client->sent >= client->response.size()) done = true;
        }
        if (TimeUtil::getMilliseconds() - client->startTime > kClientTimeoutMilliseconds) done = true;
        if (done) {
            platform::closeSocket(socket);
            client = clients_.erase(client);
        } else {
            ++client;
        }
    }
}

// GET /<sha>/<file>: the file from content/update, or 404.
void ContentHttpServer::respond(Client &client) {
    const auto line = client.request.substr(0, client.request.find("\r\n"));
    const auto methodEnd = line.find(' ');
    const auto targetEnd = line.rfind(' ');
    const auto method = line.substr(0, methodEnd);
    auto target = methodEnd != std::string::npos && targetEnd > methodEnd
                      ? decodeUrl(line.substr(methodEnd + 1, targetEnd - methodEnd - 1))
                      : "";
    if (const auto query = target.find('?'); query != std::string::npos) target.resize(query);
    int status = 404;
    std::string body;
    if ((method == "GET" || method == "HEAD") && !target.empty() && target[0] == '/' &&
        target.find("..") == std::string::npos && target.find('\\') == std::string::npos) {
        auto path = fs::path(updateDirectory_) / fs::u8path(target.substr(1));
        // The current content can always be fetched from res itself (e.g. the game's own files, for a client that
        // has to go back from an earlier patch: the game's fingerprint has no patch folder).
        const auto currentPrefix = "/" + currentSha_ + "/";
        if (!fs::is_regular_file(path) && target.rfind(currentPrefix, 0) == 0)
            path = fs::path(resourceDirectory_) / fs::u8path(target.substr(currentPrefix.size()));
        if (fs::is_regular_file(path)) {
            status = 200;
            body = FileUtil::readFile(path);
        }
    }
    const auto size = body.size();
    if (method == "HEAD") body.clear();
    client.response = "HTTP/1.1 " + std::string(status == 200 ? "200 OK" : "404 Not Found") +
                      "\r\nContent-Type: application/octet-stream\r\nContent-Length: " + std::to_string(size) +
                      "\r\nConnection: close\r\n\r\n" + body;
    if (status == 200)
        Debugger::print("content", "%s GET %s -> 200 (%zu bytes)", client.peer.c_str(), target.c_str(), size);
    else
        Debugger::warning("content: %s %s %s -> 404", client.peer.c_str(), method.c_str(), target.c_str());
}
} // namespace server
