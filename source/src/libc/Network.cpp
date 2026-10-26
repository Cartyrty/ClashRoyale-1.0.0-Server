// BSD sockets on the host's sockets (platform/Socket.h). Guest socket descriptors start at 600: above the host's file
// descriptors, below FD_SETSIZE (the app uses select() fd_sets).
// game.clashroyaleapp.com resolves to the local v1 server (HLE_SERVER=host[:port] overrides); other hosts fail to
// resolve, which keeps the SDKs offline.
#include "Hle.h"
#include "Log.h"
#include "Memory.h"
#include "platform/Platform.h"
#include "platform/Socket.h"
#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using hle::arg;
using hle::ret;
using platform::socket_t;
namespace {
std::mutex g_lock;
std::map<int, socket_t> g_sockets;
int g_next = 600; // below FD_SETSIZE (1024): the app uses select() fd_sets
socket_t host(int fd) {
    std::lock_guard lock(g_lock);
    auto f = g_sockets.find(fd);
    return f == g_sockets.end() ? platform::kInvalidSocket : f->second;
}
int add(socket_t s) {
    std::lock_guard lock(g_lock);
    const int fd = g_next++;
    g_sockets[fd] = s;
    return fd;
}
void setErrnoGuest(int e) {
    // errno lives in libc/Files.cpp; call ___error through the registry.
    const auto address = cpu::call(hle::resolveImport("___error"));
    mem::w32(address, std::uint32_t(e));
}
// The guest sees Darwin errno values (host values differ above 34, e.g. EINPROGRESS is 115 on Linux).
int darwinError(platform::NetError e) {
    using platform::NetError;
    if (e == NetError::WouldBlock) return 35; // EAGAIN / EWOULDBLOCK
    if (e == NetError::InProgress) return 36;
    if (e == NetError::Already) return 37;
    if (e == NetError::IsConnected) return 56;
    if (e == NetError::NotConnected) return 57;
    if (e == NetError::TimedOut) return 60;
    if (e == NetError::ConnectionRefused) return 61;
    if (e == NetError::HostUnreachable) return 65;
    if (e == NetError::NetUnreachable) return 51;
    if (e == NetError::ConnectionReset) return 54;
    return 5; // EIO
}
void fail() { setErrnoGuest(darwinError(platform::lastNetError())); ret(~0ull); }
// Darwin sockaddr_in: {u8 len, u8 family, u16 port, u32 addr, 8 zero}; the host's: {u16 family, u16 port, u32 addr}.
sockaddr_in fromGuest(std::uint64_t a) {
    sockaddr_in s{};
    s.sin_family = AF_INET;
    s.sin_port = mem::read<std::uint16_t>(a + 2);
    s.sin_addr.s_addr = mem::r32(a + 4);
    return s;
}
void toGuest(std::uint64_t a, const sockaddr_in &s) {
    std::memset(mem::base() + a, 0, 16);
    mem::write<std::uint8_t>(a, 16);
    mem::write<std::uint8_t>(a + 1, 2);
    mem::write<std::uint16_t>(a + 2, s.sin_port);
    mem::w32(a + 4, s.sin_addr.s_addr);
}
// Darwin socket option levels and names as the host's (Windows uses the same BSD numbers; Linux does not).
// SOL_SOCKET 0xffff; SO_NOSIGPIPE 0x1022 has no host equivalent (sends use platform::kNoSignal instead).
void hostOption(int level, int option, int &hostLevel, int &hostOption) {
    hostLevel = level;
    hostOption = option;
    if (level != 0xffff) return; // IPPROTO_TCP (6) / TCP_NODELAY (1) and IPPROTO_IP are the same everywhere
    hostLevel = SOL_SOCKET;
    static const std::map<int, int> names = {
        {0x0004, SO_REUSEADDR}, {0x0008, SO_KEEPALIVE}, {0x0020, SO_BROADCAST}, {0x0080, SO_LINGER},
        {0x1001, SO_SNDBUF},    {0x1002, SO_RCVBUF},    {0x1005, SO_SNDTIMEO},  {0x1006, SO_RCVTIMEO},
        {0x1007, SO_ERROR},     {0x1008, SO_TYPE},
    };
    if (auto found = names.find(option); found != names.end()) hostOption = found->second;
}
} // namespace

HLE(socket) {
    platform::initSockets();
    if (arg(0) != 2) { setErrnoGuest(47); return ret(~0ull); } // AF_INET only
    const socket_t s = ::socket(AF_INET, int(arg(1)), int(arg(2)));
    if (s == platform::kInvalidSocket) return fail();
    const int fd = add(s);
    hlog("net", "socket -> %d", fd);
    ret(std::uint64_t(fd));
}
HLE(socketpair) {
    // A connected loopback TCP pair.
    platform::initSockets();
    socket_t listener = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    platform::socklen len = sizeof a;
    ::bind(listener, reinterpret_cast<sockaddr *>(&a), sizeof a);
    ::listen(listener, 1);
    ::getsockname(listener, reinterpret_cast<sockaddr *>(&a), &len);
    socket_t one = ::socket(AF_INET, SOCK_STREAM, 0);
    ::connect(one, reinterpret_cast<sockaddr *>(&a), sizeof a);
    socket_t two = ::accept(listener, nullptr, nullptr);
    platform::closeSocket(listener);
    mem::w32(arg(3), std::uint32_t(add(one)));
    mem::w32(arg(3) + 4, std::uint32_t(add(two)));
    ret(0);
}
HLE(connect) {
    const auto s = fromGuest(arg(1));
    char ip[32];
    inet_ntop(AF_INET, &s.sin_addr, ip, sizeof ip);
    hlog("net", "connect %d -> %s:%d", int(arg(0)), ip, ntohs(s.sin_port));
    if (::connect(host(int(arg(0))), reinterpret_cast<const sockaddr *>(&s), sizeof s) != 0) return fail();
    ret(0);
}
HLE(bind) {
    const auto s = fromGuest(arg(1));
    if (::bind(host(int(arg(0))), reinterpret_cast<const sockaddr *>(&s), sizeof s) != 0) return fail();
    ret(0);
}
HLE(listen) { ret(::listen(host(int(arg(0))), int(arg(1))) == 0 ? 0 : ~0ull); }
// Message headers (type u16, length u24, version u16, big-endian) in the client's stream, logged under [msg].
static void logMessages(const char *direction, int fd, const std::uint8_t *data, int n) {
    static std::map<std::string, std::pair<std::vector<std::uint8_t>, std::size_t>> streams; // pending bytes, bytes left in body
    if (!logEnabled("msg") || n <= 0) return;
    static std::mutex lock;
    std::lock_guard guard(lock);
    auto &[buffer, skip] = streams[std::string(direction) + std::to_string(fd)];
    for (int i = 0; i < n; ++i) {
        if (skip) { --skip; continue; }
        buffer.push_back(data[i]);
        if (buffer.size() == 7) {
            const unsigned type = (buffer[0] << 8) | buffer[1];
            const std::size_t length = (std::size_t(buffer[2]) << 16) | (buffer[3] << 8) | buffer[4];
            if (type != 10108 && type != 20108) hlog("msg", "%s %u (%zu bytes)", direction, type, length); // skip heartbeats
            skip = length;
            buffer.clear();
        }
    }
}
HLE(send) {
    logMessages("client ->", int(arg(0)), mem::base() + arg(1), int(arg(2)));
    const int n = int(::send(host(int(arg(0))), reinterpret_cast<const char *>(mem::base() + arg(1)), int(arg(2)), platform::kNoSignal));
    if (n < 0) return fail();
    ret(std::uint64_t(n));
}
// sendto(fd, buffer, length, flags, address, address length): UDP datagrams go to the address; without one (a
// connected socket) it is send.
HLE(sendto) {
    const auto data = reinterpret_cast<const char *>(mem::base() + arg(1));
    int n;
    if (arg(4)) {
        const auto to = fromGuest(arg(4));
        n = int(::sendto(host(int(arg(0))), data, int(arg(2)), platform::kNoSignal, reinterpret_cast<const sockaddr *>(&to), sizeof to));
    } else {
        n = int(::send(host(int(arg(0))), data, int(arg(2)), platform::kNoSignal));
    }
    if (n < 0) return fail();
    ret(std::uint64_t(n));
}
HLE(recv) {
    const int n = int(::recv(host(int(arg(0))), reinterpret_cast<char *>(mem::base() + arg(1)), int(arg(2)), 0));
    if (n < 0) return fail();
    logMessages("server ->", int(arg(0)), mem::base() + arg(1), n);
    ret(std::uint64_t(n));
}
// recvfrom(fd, buffer, length, flags, address, address length): the sender's address is reported (the game's UDP
// socket only accepts datagrams from the server's port).
HLE(recvfrom) {
    sockaddr_in from{};
    platform::socklen fromLength = sizeof from;
    const int n = int(::recvfrom(host(int(arg(0))), reinterpret_cast<char *>(mem::base() + arg(1)), int(arg(2)), 0,
                                 reinterpret_cast<sockaddr *>(&from), &fromLength));
    if (n < 0) return fail();
    if (arg(4)) {
        toGuest(arg(4), from);
        if (arg(5)) mem::w32(arg(5), 16);
    }
    ret(std::uint64_t(n));
}
HLE(shutdown) { ret(::shutdown(host(int(arg(0))), int(arg(1))) == 0 ? 0 : ~0ull); }
HLE(getsockname) {
    sockaddr_in s{};
    platform::socklen len = sizeof s;
    if (::getsockname(host(int(arg(0))), reinterpret_cast<sockaddr *>(&s), &len) != 0) return fail();
    toGuest(arg(1), s);
    if (arg(2)) mem::w32(arg(2), 16);
    ret(0);
}
HLE(getpeername) {
    sockaddr_in s{};
    platform::socklen len = sizeof s;
    if (::getpeername(host(int(arg(0))), reinterpret_cast<sockaddr *>(&s), &len) != 0) return fail();
    toGuest(arg(1), s);
    if (arg(2)) mem::w32(arg(2), 16);
    ret(0);
}
HLE(setsockopt) {
    const int level = int(arg(1)), option = int(arg(2));
    if (level == 0xffff && option == 0x1022) return ret(0); // SO_NOSIGPIPE
    int hostLevel, hostName;
    hostOption(level, option, hostLevel, hostName);
    ret(::setsockopt(host(int(arg(0))), hostLevel, hostName, reinterpret_cast<const char *>(mem::base() + arg(3)), platform::socklen(arg(4))) == 0 ? 0 : ~0ull);
}
HLE(getsockopt) {
    platform::socklen len = platform::socklen(mem::r32(arg(4)));
    int hostLevel, hostName;
    hostOption(int(arg(1)), int(arg(2)), hostLevel, hostName);
    const int r = ::getsockopt(host(int(arg(0))), hostLevel, hostName, reinterpret_cast<char *>(mem::base() + arg(3)), &len);
    if (int(arg(1)) == 0xffff && int(arg(2)) == 0x1007 && r == 0) { // SO_ERROR: translate
        const int e = int(mem::r32(arg(3)));
        mem::w32(arg(3), std::uint32_t(e ? darwinError(platform::netError(e)) : 0));
    }
    mem::w32(arg(4), std::uint32_t(len));
    ret(r == 0 ? 0 : ~0ull);
}
// fcntl on sockets: F_GETFL 3 / F_SETFL 4 with O_NONBLOCK 4.
HLE(fcntl) {
    const int fd = int(arg(0)), cmd = int(arg(1));
    const socket_t s = host(fd);
    if (s == platform::kInvalidSocket) return ret(0);
    static std::map<int, bool> nonblocking;
    if (cmd == 3) return ret(nonblocking[fd] ? 4 : 0);
    if (cmd == 4) {
        const auto flags = mem::r64(cpu::sp()); // variadic int on the stack
        const bool on = (flags & 4) != 0;
        platform::setNonBlocking(s, on);
        nonblocking[fd] = on;
        return ret(0);
    }
    ret(0);
}
// close() must also close sockets; files go to the host.
HLE(close) {
    const int fd = int(arg(0));
    {
        std::lock_guard lock(g_lock);
        auto f = g_sockets.find(fd);
        if (f != g_sockets.end()) {
            platform::closeSocket(f->second);
            g_sockets.erase(f);
            return ret(0);
        }
    }
    ret(std::uint64_t(std::int64_t(platform::closeDescriptor(fd))));
}
// select(nfds, readfds, writefds, errorfds, timeout): Darwin fd_set is a 1024-bit int32 array; timeval {i64 seconds,
// i32 microseconds}.
static int selectImpl() {
    const int nfds = int(arg(0));
    const std::uint64_t sets[3] = {arg(1), arg(2), arg(3)};
    auto isSet = [](std::uint64_t set, int fd) { return set && (mem::r32(set + 4 * std::uint64_t(fd / 32)) & (1u << (fd % 32))); };
    std::vector<platform::SocketWait> waits;
    std::vector<int> fds;
    for (int fd = 0; fd < nfds; ++fd) {
        platform::SocketWait w;
        w.read = isSet(sets[0], fd);
        w.write = isSet(sets[1], fd);
        w.except = isSet(sets[2], fd);
        if (!w.read && !w.write && !w.except) continue;
        w.socket = host(fd);
        if (w.socket == platform::kInvalidSocket) continue;
        waits.push_back(w);
        fds.push_back(fd);
    }
    int timeoutMs = -1;
    if (arg(4)) timeoutMs = int(std::int64_t(mem::r64(arg(4))) * 1000 + (std::int64_t(mem::r32(arg(4) + 8)) + 999) / 1000);
    for (auto set : sets) if (set) std::memset(mem::base() + set, 0, std::size_t((nfds + 31) / 32 * 4));
    if (waits.empty()) {
        if (timeoutMs > 0) std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        return 0;
    }
    const int n = platform::waitSockets(waits, timeoutMs);
    if (n <= 0) return n;
    auto mark = [](std::uint64_t set, int fd) { mem::w32(set + 4 * std::uint64_t(fd / 32), mem::r32(set + 4 * std::uint64_t(fd / 32)) | (1u << (fd % 32))); };
    for (std::size_t i = 0; i < waits.size(); ++i) {
        if (waits[i].readable) mark(sets[0], fds[i]);
        if (waits[i].writable) mark(sets[1], fds[i]);
        if (waits[i].error) mark(sets[2], fds[i]);
    }
    return n;
}
static hle::Registrar r_select("_select$DARWIN_EXTSN", [] { ret(std::uint64_t(std::int64_t(selectImpl()))); });
static hle::Registrar r_select2("_select", [] { ret(std::uint64_t(std::int64_t(selectImpl()))); });

// getaddrinfo(node, service, hints, res): Darwin addrinfo {flags, family, socktype, protocol, addrlen, canonname +24,
// addr +32, next +40}.
HLE(getaddrinfo) {
    platform::initSockets();
    std::string node = arg(0) ? mem::cstring(arg(0)) : "", service = arg(1) ? mem::cstring(arg(1)) : "";
    std::string target = node;
    if (node.find("clashroyale") != std::string::npos) {
        target = "127.0.0.1";
        if (const char *override = std::getenv("HLE_SERVER")) {
            std::string o = override;
            const auto colon = o.find(':');
            target = o.substr(0, colon);
            if (colon != std::string::npos) service = o.substr(colon + 1);
        }
    } else if (!node.empty() && node != "localhost" && node.find_first_not_of("0123456789.") != std::string::npos) {
        hlog("net", "getaddrinfo %s: not resolved (offline)", node.c_str());
        return ret(8); // EAI_NONAME
    }
    hlog("net", "getaddrinfo %s:%s -> %s", node.c_str(), service.c_str(), target.c_str());
    addrinfo hints{}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = arg(2) ? int(mem::r32(arg(2) + 8)) : SOCK_STREAM;
    if (::getaddrinfo(target.c_str(), service.empty() ? nullptr : service.c_str(), &hints, &result) != 0 || !result) return ret(8);
    std::uint64_t first = 0, previous = 0;
    for (auto *r = result; r; r = r->ai_next) {
        if (r->ai_family != AF_INET) continue;
        const auto node_ = mem::calloc(48 + 16);
        mem::w32(node_ + 4, 2);
        mem::w32(node_ + 8, std::uint32_t(r->ai_socktype));
        mem::w32(node_ + 12, std::uint32_t(r->ai_protocol));
        mem::w32(node_ + 16, 16);
        toGuest(node_ + 48, *reinterpret_cast<sockaddr_in *>(r->ai_addr));
        mem::w64(node_ + 32, node_ + 48);
        if (previous) mem::w64(previous + 40, node_);
        else first = node_;
        previous = node_;
    }
    ::freeaddrinfo(result);
    mem::w64(arg(3), first);
    ret(first ? 0 : 8);
}
HLE(freeaddrinfo) {}
HLE(gai_strerror) { static std::uint64_t s = mem::hleString("host not found"); ret(s); }
HLE(inet_ntop) {
    char out[64] = {};
    if (arg(0) == 2) inet_ntop(AF_INET, mem::base() + arg(1), out, sizeof out);
    std::strncpy(reinterpret_cast<char *>(mem::base() + arg(2)), out, std::size_t(arg(3)));
    ret(arg(2));
}
HLE(inet_addr) {
    in_addr a{};
    inet_pton(AF_INET, mem::cstring(arg(0)).c_str(), &a);
    ret(a.s_addr);
}
HLE(htons) { ret(htons(std::uint16_t(arg(0)))); }
HLE(ntohs) { ret(ntohs(std::uint16_t(arg(0)))); }
