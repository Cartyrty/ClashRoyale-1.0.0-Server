// platform/Platform.h on Linux (POSIX): the services the server needs too. No SDL here - the server does not link it;
// the window registers its event wait (setEventWaiter) when it is created.
#include "platform/Platform.h"
#include <fcntl.h>
#include <spawn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>

extern char **environ;

namespace platform {
namespace {
// Committed pages of the (single) reserved range, one bit per 4 KB page: Linux has no VirtualQuery.
std::uint8_t *g_reserved = nullptr;
std::uint64_t g_reservedSize = 0;
std::vector<std::uint64_t> g_committed;
std::mutex g_memoryLock;
std::atomic<void (*)(int)> g_eventWaiter{nullptr};
void fill(FileStat &out, const struct stat &st) {
    out.directory = S_ISDIR(st.st_mode);
    out.size = std::int64_t(st.st_size);
    out.modified = std::int64_t(st.st_mtime);
}
} // namespace

void *reserveAddressSpace(std::uint64_t size) {
    void *p = mmap(nullptr, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED) return nullptr;
    std::lock_guard lock(g_memoryLock);
    g_reserved = static_cast<std::uint8_t *>(p);
    g_reservedSize = size;
    g_committed.assign(std::size_t((size / 4096 + 63) / 64), 0);
    return p;
}
bool commitMemory(void *address, std::uint64_t size) {
    if (mprotect(address, size, PROT_READ | PROT_WRITE) != 0) return false;
    std::lock_guard lock(g_memoryLock);
    const auto first = std::uint64_t(static_cast<std::uint8_t *>(address) - g_reserved) / 4096;
    for (std::uint64_t page = first; page < first + (size + 4095) / 4096; ++page) g_committed[page / 64] |= 1ull << (page % 64);
    return true;
}
bool isCommitted(const void *address) {
    std::lock_guard lock(g_memoryLock);
    const auto *p = static_cast<const std::uint8_t *>(address);
    if (p < g_reserved || p >= g_reserved + g_reservedSize) return false;
    const auto page = std::uint64_t(p - g_reserved) / 4096;
    return (g_committed[page / 64] >> (page % 64)) & 1;
}

std::uint32_t currentThreadId() { return std::uint32_t(syscall(SYS_gettid)); }
std::uint64_t milliseconds() {
    return std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
void setEventWaiter(void (*waiter)(int ms)) { g_eventWaiter = waiter; }
void waitForEvents(int ms) {
    if (auto waiter = g_eventWaiter.load()) waiter(std::max(0, ms));
    else std::this_thread::sleep_for(std::chrono::milliseconds(std::max(0, ms)));
}

// xdg-open, started detached (no shell: the URL is one argument).
bool openUrl(const std::string &utf8Url) {
    std::string program = "xdg-open", url = utf8Url;
    char *argv[] = {program.data(), url.data(), nullptr};
    pid_t pid = 0;
    return posix_spawnp(&pid, "xdg-open", nullptr, nullptr, argv, environ) == 0;
}

FILE *openFile(const std::string &utf8Path, const char *mode) { return std::fopen(utf8Path.c_str(), mode); }
int seekFile(FILE *file, std::int64_t offset, int whence) { return fseeko(file, off_t(offset), whence); }
std::int64_t tellFile(FILE *file) { return std::int64_t(ftello(file)); }
int fileDescriptor(FILE *file) { return fileno(file); }
int openDescriptor(const std::string &utf8Path, unsigned flags) {
    int host = (flags & kRead) && (flags & kWrite) ? O_RDWR : (flags & kWrite) ? O_WRONLY : O_RDONLY;
    if (flags & kAppend) host |= O_APPEND;
    if (flags & kCreate) host |= O_CREAT;
    if (flags & kTruncate) host |= O_TRUNC;
    if (flags & kExclusive) host |= O_EXCL;
    return open(utf8Path.c_str(), host | O_CLOEXEC, 0644);
}
int createFile(const std::string &utf8Path) { return openDescriptor(utf8Path, kRead | kWrite | kCreate); }
std::int64_t readDescriptor(int fd, void *data, std::size_t size) { return std::int64_t(read(fd, data, size)); }
std::int64_t writeDescriptor(int fd, const void *data, std::size_t size) { return std::int64_t(write(fd, data, size)); }
std::int64_t seekDescriptor(int fd, std::int64_t offset, int whence) { return std::int64_t(lseek(fd, off_t(offset), whence)); }
int truncateDescriptor(int fd, std::int64_t size) { return ftruncate(fd, off_t(size)); }
int closeDescriptor(int fd) { return close(fd); }
bool statPath(const std::string &utf8Path, FileStat &out) {
    struct stat st;
    if (stat(utf8Path.c_str(), &st) != 0) return false;
    fill(out, st);
    return true;
}
bool statDescriptor(int fd, FileStat &out) {
    struct stat st;
    if (fstat(fd, &st) != 0) return false;
    fill(out, st);
    return true;
}

std::tm utcTime(std::time_t t) {
    std::tm tm{};
    gmtime_r(&t, &tm);
    return tm;
}
std::tm localTime(std::time_t t) {
    std::tm tm{};
    localtime_r(&t, &tm);
    return tm;
}
std::time_t fromUtcTime(std::tm t) { return timegm(&t); }
long utcOffsetSeconds() {
    tzset();
    return -timezone;
}
} // namespace platform
