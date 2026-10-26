#pragma once
// Operating-system services the emulator needs besides the window (Window.h), audio output (AudioOutput.h) and
// compressed-audio decoding (MediaDecode.h). Each is implemented once per host: platform/win32 (Windows API) and
// platform/sdl (Linux: SDL2 and POSIX). System.cpp holds what the server needs too (memory, threads, time, files,
// URLs); App.cpp what only the client needs (GL, fonts, alerts).
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace platform {
// Address space: reserve inaccessible, commit read/write (zero-filled), query.
void *reserveAddressSpace(std::uint64_t size);
bool commitMemory(void *address, std::uint64_t size);
bool isCommitted(const void *address);

std::uint32_t currentThreadId();
std::uint64_t milliseconds(); // monotonic
// Sleeps up to `ms` milliseconds, returning early when window input arrives.
void waitForEvents(int ms);
// Linux: the window registers how to wait for its input (SDL lives in the client only); without one, waits sleep.
// Windows waits on window messages directly and ignores this.
void setEventWaiter(void (*waiter)(int ms));

// Opens an http(s) URL in the user's browser (detached); false if that failed to start.
bool openUrl(const std::string &utf8Url);

// stdio with UTF-8 paths and 64-bit offsets.
FILE *openFile(const std::string &utf8Path, const char *mode);
int seekFile(FILE *file, std::int64_t offset, int whence);
std::int64_t tellFile(FILE *file);
int fileDescriptor(FILE *file);
// File descriptors (binary): open flags, read / write / seek / truncate, and stat.
enum OpenFlags : unsigned { kRead = 1, kWrite = 2, kAppend = 4, kCreate = 8, kTruncate = 16, kExclusive = 32 };
int openDescriptor(const std::string &utf8Path, unsigned flags); // -1 on failure
int createFile(const std::string &utf8Path);                     // kRead | kWrite | kCreate
std::int64_t readDescriptor(int fd, void *data, std::size_t size);
std::int64_t writeDescriptor(int fd, const void *data, std::size_t size);
std::int64_t seekDescriptor(int fd, std::int64_t offset, int whence);
int truncateDescriptor(int fd, std::int64_t size);
int closeDescriptor(int fd);
struct FileStat {
    bool directory = false;
    std::int64_t size = 0;
    std::int64_t modified = 0; // seconds since 1970
};
bool statPath(const std::string &utf8Path, FileStat &out);
bool statDescriptor(int fd, FileStat &out);

std::tm utcTime(std::time_t t);
std::tm localTime(std::time_t t);
std::time_t fromUtcTime(std::tm t); // timegm
long utcOffsetSeconds();            // local time minus UTC (standard time)

// --- Client only (App.cpp) ---
// An OpenGL function of the current context (extensions and core functions past the platform's link library).
void *glProc(const char *name);
// The first of the font files (by file name, searched case-insensitively in the system's font folders), or "".
std::string findFont(const std::vector<std::string> &fileNames);
// The clipboard's text (UTF-8), or "".
std::string clipboardText();
// A modal message box with one button per label (at least one); returns the index of the button pressed.
int showAlert(const std::string &title, const std::string &message, const std::vector<std::string> &buttons);
} // namespace platform
