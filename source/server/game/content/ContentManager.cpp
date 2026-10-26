#include "game/content/ContentManager.h"

#include "game/utils/FileUtil.h"
#include "titan/logic/debug/Debugger.h"

#include <filesystem>

namespace server {
namespace fs = std::filesystem;

// Nothing is hashed or built here.
ContentManager::ContentManager(const std::string &resourceDirectory, const std::string &dataDirectory,
                               const std::string &urlOverride)
    : urlOverride_(urlOverride) {
    auto fingerprint = FileUtil::readJson(fs::path(dataDirectory) / "content" / "fingerprint.json");
    const bool patched = !fingerprint.isNull();
    if (!patched) fingerprint = FileUtil::readJson(fs::path(resourceDirectory) / "fingerprint.json");
    if (fingerprint.isNull()) {
        Debugger::warning("content: no fingerprint.json in %s; content updates are off", resourceDirectory.c_str());
        return;
    }
    sha_ = fingerprint["sha"].str("");
    fingerprint_ = fingerprint.dump();
    Debugger::print("content", "fingerprint %s (version %s): %s", sha_.c_str(), fingerprint["version"].str("").c_str(),
                    patched ? "the content patch" : "the game's own content (no patch built)");
}

std::string ContentManager::getContentUrl(const std::string &serverAddress) const {
    if (!urlOverride_.empty()) return urlOverride_;
    return "http://" + serverAddress + ":" + std::to_string(port_);
}
} // namespace server
