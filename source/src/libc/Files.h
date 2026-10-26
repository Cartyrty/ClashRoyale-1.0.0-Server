#pragma once
#include <cstdint>
#include <string>
// iOS paths seen by the app and their host locations:
//   kBundlePath (the .app)          -> the res directory
//   kHomePath (the data container)  -> <exe dir>/sandbox (Documents, Library/Caches, Library/Preferences, tmp)
namespace files {
extern const char *const kBundlePath;
extern const char *const kHomePath;
std::string hostPath(const std::string &guestPath);
void setSandboxRoot(const std::string &path);
const std::string &sandboxRoot();
bool readWholeFile(const std::string &guestPath, std::string &out);
} // namespace files
