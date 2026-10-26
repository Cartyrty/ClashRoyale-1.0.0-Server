#pragma once
// Sockets on either host: Winsock on Windows, BSD sockets on Linux. Shared code uses socket_t / kInvalidSocket, the
// helpers below for what differs (startup, close, non-blocking mode, errors, SIGPIPE, waiting, listener options), and
// the standard calls (socket, connect, send, recv, getaddrinfo...) for the rest. Errors come back as NetError, which
// callers map to whatever their side needs (Darwin errno values for the guest).
#ifdef _WIN32
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/select.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif
#include <cstddef>
#include <vector>

namespace platform {
#ifdef _WIN32
using socket_t = SOCKET;
using socklen = int;
inline constexpr socket_t kInvalidSocket = INVALID_SOCKET;
inline constexpr int kNoSignal = 0;
#else
using socket_t = int;
using socklen = socklen_t;
inline constexpr socket_t kInvalidSocket = -1;
inline constexpr int kNoSignal = MSG_NOSIGNAL; // a send to a closed peer fails with EPIPE instead of killing the process
#endif

enum class NetError { None, WouldBlock, InProgress, Already, IsConnected, NotConnected, ConnectionRefused, ConnectionReset,
                      TimedOut, HostUnreachable, NetUnreachable, Other };

// Once per process: WSAStartup on Windows; on Linux SIGPIPE is ignored (a dropped peer must not end the process).
inline void initSockets() {
    static const bool started = [] {
#ifdef _WIN32
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
#else
        signal(SIGPIPE, SIG_IGN);
#endif
        return true;
    }();
    (void)started;
}
inline int closeSocket(socket_t s) {
#ifdef _WIN32
    return closesocket(s);
#else
    return close(s);
#endif
}
inline bool setNonBlocking(socket_t s, bool on) {
#ifdef _WIN32
    u_long mode = on ? 1 : 0;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
#else
    const int flags = fcntl(s, F_GETFL, 0);
    return flags >= 0 && fcntl(s, F_SETFL, on ? flags | O_NONBLOCK : flags & ~O_NONBLOCK) == 0;
#endif
}
// A host error code (WSAGetLastError / errno, or an SO_ERROR value) as a NetError.
inline NetError netError(int code) {
    if (code == 0) return NetError::None;
#ifdef _WIN32
    if (code == WSAEWOULDBLOCK) return NetError::WouldBlock;
    if (code == WSAEINPROGRESS) return NetError::InProgress;
    if (code == WSAEALREADY) return NetError::Already;
    if (code == WSAEISCONN) return NetError::IsConnected;
    if (code == WSAENOTCONN) return NetError::NotConnected;
    if (code == WSAECONNREFUSED) return NetError::ConnectionRefused;
    if (code == WSAECONNRESET) return NetError::ConnectionReset;
    if (code == WSAETIMEDOUT) return NetError::TimedOut;
    if (code == WSAEHOSTUNREACH) return NetError::HostUnreachable;
    if (code == WSAENETUNREACH) return NetError::NetUnreachable;
#else
    // If-chains, not a switch: EAGAIN == EWOULDBLOCK on Linux.
    if (code == EAGAIN || code == EWOULDBLOCK) return NetError::WouldBlock;
    if (code == EINPROGRESS) return NetError::InProgress;
    if (code == EALREADY) return NetError::Already;
    if (code == EISCONN) return NetError::IsConnected;
    if (code == ENOTCONN) return NetError::NotConnected;
    if (code == ECONNREFUSED) return NetError::ConnectionRefused;
    if (code == ECONNRESET || code == EPIPE) return NetError::ConnectionReset;
    if (code == ETIMEDOUT) return NetError::TimedOut;
    if (code == EHOSTUNREACH) return NetError::HostUnreachable;
    if (code == ENETUNREACH) return NetError::NetUnreachable;
#endif
    return NetError::Other;
}
inline int lastNetErrorCode() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}
inline NetError lastNetError() { return netError(lastNetErrorCode()); }

// A UDP socket serving many peers: on Windows an ICMP "port unreachable" for an earlier sendto would otherwise fail the
// next recvfrom with WSAECONNRESET (SIO_UDP_CONNRESET off); Linux reports nothing on an unconnected socket.
inline void prepareDatagramSocket(socket_t s) {
#ifdef _WIN32
    BOOL report = FALSE;
    DWORD returned = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &report, sizeof report, nullptr, 0, &returned, nullptr, nullptr);
#else
    (void)s;
#endif
}

// A listening socket's address option: on Windows exclusive use (a second server on the port fails instead of sharing
// it); on Linux SO_REUSEADDR (a restarted server binds at once instead of waiting out TIME_WAIT; Linux never lets two
// listeners share the port this way).
inline void prepareListener(socket_t s) {
#ifdef _WIN32
    BOOL exclusive = TRUE;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char *>(&exclusive), sizeof exclusive);
#else
    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
#endif
}

// A blocking receive gives up after `ms` milliseconds (Windows takes a DWORD of milliseconds, Linux a timeval).
inline void setReceiveTimeout(socket_t s, int ms) {
#ifdef _WIN32
    DWORD timeout = DWORD(ms);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof timeout);
#else
    timeval timeout{ms / 1000, (ms % 1000) * 1000};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
#endif
}

// Waits for sockets (select on Windows, poll on Linux: descriptors there may exceed FD_SETSIZE). A non-blocking
// connect that failed reports writable and error (as Darwin does); readable also covers a closed peer.
struct SocketWait {
    socket_t socket = kInvalidSocket;
    bool read = false, write = false, except = false;    // what to wait for
    bool readable = false, writable = false, error = false; // results
};
// timeoutMs < 0: no timeout. Returns how many sockets are ready, or -1.
inline int waitSockets(std::vector<SocketWait> &items, int timeoutMs) {
#ifdef _WIN32
    fd_set sets[3];
    for (auto &set : sets) FD_ZERO(&set);
    for (auto &w : items) {
        if (w.read) FD_SET(w.socket, &sets[0]);
        if (w.write) FD_SET(w.socket, &sets[1]);
        if (w.except) FD_SET(w.socket, &sets[2]);
    }
    timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    const int n = select(0, &sets[0], &sets[1], &sets[2], timeoutMs < 0 ? nullptr : &tv);
    int ready = 0;
    for (auto &w : items) {
        w.readable = n > 0 && w.read && FD_ISSET(w.socket, &sets[0]);
        w.writable = n > 0 && w.write && FD_ISSET(w.socket, &sets[1]);
        w.error = n > 0 && w.except && FD_ISSET(w.socket, &sets[2]);
        ready += w.readable || w.writable || w.error;
    }
    return n < 0 ? -1 : ready;
#else
    std::vector<pollfd> fds;
    for (auto &w : items) fds.push_back({w.socket, short((w.read ? POLLIN : 0) | (w.write ? POLLOUT : 0) | (w.except ? POLLPRI : 0)), 0});
    const int n = poll(fds.data(), nfds_t(fds.size()), timeoutMs);
    int ready = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        auto &w = items[i];
        const short r = n > 0 ? fds[i].revents : 0;
        const bool failed = (r & (POLLERR | POLLHUP)) != 0;
        w.readable = w.read && (r & (POLLIN | POLLHUP)) != 0;
        w.writable = w.write && ((r & POLLOUT) != 0 || failed);
        w.error = w.except && ((r & POLLPRI) != 0 || (r & POLLERR) != 0);
        ready += w.readable || w.writable || w.error;
    }
    return n < 0 ? -1 : ready;
#endif
}
} // namespace platform
