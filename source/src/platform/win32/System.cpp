// platform/Platform.h on Windows: the services the server needs too (memory, threads, time, files, URLs).
#include "platform/Platform.h"
#include <windows.h>
#include <shellapi.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <algorithm>
#include <string>

namespace platform {
namespace {
std::wstring wide(const std::string &utf8) {
    if (utf8.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
    std::wstring out(std::size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), out.data(), n);
    return out;
}
void fill(FileStat &out, const struct _stat64 &st) {
    out.directory = (st.st_mode & _S_IFDIR) != 0;
    out.size = std::int64_t(st.st_size);
    out.modified = std::int64_t(st.st_mtime);
}
} // namespace

void *reserveAddressSpace(std::uint64_t size) { return VirtualAlloc(nullptr, SIZE_T(size), MEM_RESERVE, PAGE_NOACCESS); }
bool commitMemory(void *address, std::uint64_t size) { return VirtualAlloc(address, SIZE_T(size), MEM_COMMIT, PAGE_READWRITE) != nullptr; }
bool isCommitted(const void *address) {
    MEMORY_BASIC_INFORMATION info{};
    return VirtualQuery(address, &info, sizeof info) && info.State == MEM_COMMIT;
}

std::uint32_t currentThreadId() { return GetCurrentThreadId(); }
std::uint64_t milliseconds() { return GetTickCount64(); }
// MsgWaitForMultipleObjects returns early on window input.
void waitForEvents(int ms) { MsgWaitForMultipleObjects(0, nullptr, FALSE, DWORD(std::max(0, ms)), QS_ALLINPUT); }
void setEventWaiter(void (*)(int)) {}

bool openUrl(const std::string &utf8Url) {
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", wide(utf8Url).c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

FILE *openFile(const std::string &utf8Path, const char *mode) { return _wfopen(wide(utf8Path).c_str(), wide(mode).c_str()); }
int seekFile(FILE *file, std::int64_t offset, int whence) { return _fseeki64(file, offset, whence); }
std::int64_t tellFile(FILE *file) { return _ftelli64(file); }
int fileDescriptor(FILE *file) { return _fileno(file); }
int openDescriptor(const std::string &utf8Path, unsigned flags) {
    int host = _O_BINARY;
    if ((flags & kRead) && (flags & kWrite)) host |= _O_RDWR;
    else if (flags & kWrite) host |= _O_WRONLY;
    else host |= _O_RDONLY;
    if (flags & kAppend) host |= _O_APPEND;
    if (flags & kCreate) host |= _O_CREAT;
    if (flags & kTruncate) host |= _O_TRUNC;
    if (flags & kExclusive) host |= _O_EXCL;
    return _wopen(wide(utf8Path).c_str(), host, _S_IREAD | _S_IWRITE);
}
int createFile(const std::string &utf8Path) { return openDescriptor(utf8Path, kRead | kWrite | kCreate); }
std::int64_t readDescriptor(int fd, void *data, std::size_t size) { return _read(fd, data, unsigned(size)); }
std::int64_t writeDescriptor(int fd, const void *data, std::size_t size) { return _write(fd, data, unsigned(size)); }
std::int64_t seekDescriptor(int fd, std::int64_t offset, int whence) { return _lseeki64(fd, offset, whence); }
int truncateDescriptor(int fd, std::int64_t size) { return _chsize_s(fd, size); }
int closeDescriptor(int fd) { return _close(fd); }
bool statPath(const std::string &utf8Path, FileStat &out) {
    struct _stat64 st;
    if (_wstat64(wide(utf8Path).c_str(), &st) != 0) return false;
    fill(out, st);
    return true;
}
bool statDescriptor(int fd, FileStat &out) {
    struct _stat64 st;
    if (_fstat64(fd, &st) != 0) return false;
    fill(out, st);
    return true;
}

std::tm utcTime(std::time_t t) {
    std::tm tm{};
    gmtime_s(&tm, &t);
    return tm;
}
std::tm localTime(std::time_t t) {
    std::tm tm{};
    localtime_s(&tm, &t);
    return tm;
}
std::time_t fromUtcTime(std::tm t) { return _mkgmtime(&t); }
long utcOffsetSeconds() {
    long tz = 0;
    _get_timezone(&tz);
    return -tz;
}
} // namespace platform
