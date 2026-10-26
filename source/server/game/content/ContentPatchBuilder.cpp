#include "game/content/ContentPatchBuilder.h"

#include "game/utils/FileUtil.h"
#include "game/utils/TimeUtil.h"
#include "libc/Crypto.h"
#include "titan/logic/debug/Debugger.h"

#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace server {
namespace ContentPatchBuilder {
namespace fs = std::filesystem;

namespace {
using FileList = std::vector<std::pair<std::string, std::string>>; // file, sha (fingerprint order)

FileList getFiles(const json::Value &fingerprint) {
    FileList files;
    for (const auto &file : fingerprint["files"].items()) files.emplace_back(file["file"].str(""), file["sha"].str(""));
    return files;
}

json::Value saveFiles(const FileList &files) {
    auto saved = json::Value::array();
    for (const auto &[file, sha] : files) {
        auto entry = json::Value::object();
        entry["file"] = file;
        entry["sha"] = sha;
        saved.push(entry);
    }
    return saved;
}

// "2.1306.1" -> "2.1306.2"
std::string getNextVersion(const std::string &version) {
    const auto dot = version.rfind('.');
    const auto head = dot == std::string::npos ? std::string() : version.substr(0, dot + 1);
    const auto last = dot == std::string::npos ? version : version.substr(dot + 1);
    return head + std::to_string(std::atoi(last.c_str()) + 1);
}

std::string sha1Hex(const std::string &data) { return crypto::sha1Hex(data.data(), data.size()); }
} // namespace

int build(const std::string &masterDirectory, const std::string &gameFingerprintPath,
          const std::string &dataDirectory) {
    const auto contentDirectory = fs::path(dataDirectory) / "content";
    const auto updateDirectory = contentDirectory / "update";
    const auto gameFingerprint = FileUtil::readJson(gameFingerprintPath); // what an unpatched client has
    if (gameFingerprint.isNull()) {
        Debugger::warning("content: %s is missing", gameFingerprintPath.c_str());
        return 1;
    }
    if (!fs::is_directory(masterDirectory)) {
        Debugger::warning("content: the master assets folder %s does not exist", masterDirectory.c_str());
        return 1;
    }
    auto lastPatch = FileUtil::readJson(contentDirectory / "fingerprint.json");
    if (lastPatch.isNull()) lastPatch = gameFingerprint;
    const auto gameFiles = getFiles(gameFingerprint);
    const auto lastPatchFiles = getFiles(lastPatch);
    std::map<std::string, std::string> gameSha(gameFiles.begin(), gameFiles.end());
    std::map<std::string, std::string> lastPatchSha(lastPatchFiles.begin(), lastPatchFiles.end());

    std::map<std::string, std::string> masterSha;
    for (const auto &entry : fs::recursive_directory_iterator(masterDirectory)) {
        if (!entry.is_regular_file()) continue;
        const auto relative = fs::relative(entry.path(), masterDirectory).generic_string();
        if (relative == "fingerprint.json" || entry.path().filename() == ".DS_Store") continue;
        if (relative.find('/') == std::string::npos && !gameSha.count(relative)) continue;
        masterSha[relative] = sha1Hex(FileUtil::readFile(entry.path()));
    }
    // The game's file list with the master's hashes (a file missing there keeps its original entry), then new files.
    FileList currentFiles;
    for (const auto &[file, sha] : gameFiles)
        currentFiles.emplace_back(file, masterSha.count(file) ? masterSha[file] : sha);
    for (const auto &[file, sha] : masterSha)
        if (!gameSha.count(file)) currentFiles.emplace_back(file, sha);

    if (currentFiles == gameFiles) {
        // The game's own content: no patch (a client on an earlier patch gets the originals from res).
        std::error_code error;
        const bool hadPatch = fs::remove(contentDirectory / "fingerprint.json", error);
        Debugger::print("content", "the master assets are the game's own files: %s",
                        hadPatch ? "content patch removed" : "no patch needed");
        return 0;
    }
    if (currentFiles == lastPatchFiles) {
        Debugger::print("content", "no changes since the last patch %s (version %s)", lastPatch["sha"].str("").c_str(),
                        lastPatch["version"].str("").c_str());
        return 0;
    }
    auto fingerprint = json::Value::object();
    fingerprint["files"] = saveFiles(currentFiles);
    const auto version = getNextVersion(lastPatch["version"].str("1.0.0"));
    const auto sha = sha1Hex(fingerprint["files"].dump() +
                             std::to_string(TimeUtil::getMilliseconds())); // content.js: sha1(files + time)
    fingerprint["sha"] = sha;
    fingerprint["version"] = version;
    const auto text = fingerprint.dump();
    // The patch holds every file that differs from the game's own, and every file an earlier patch changed (a client
    // on that patch may need the new or the original copy back).
    std::set<std::string> patchFiles;
    for (const auto &[file, fileSha] : currentFiles) {
        const bool changed = !gameSha.count(file) || gameSha[file] != fileSha;
        const bool previouslyChanged =
            lastPatchSha.count(file) && gameSha.count(file) && lastPatchSha[file] != gameSha[file];
        if ((changed || previouslyChanged) && masterSha.count(file)) patchFiles.insert(file);
    }
    const auto patch = updateDirectory / sha;
    for (const auto &file : patchFiles) {
        fs::create_directories((patch / file).parent_path());
        fs::copy_file(fs::path(masterDirectory) / file, patch / file, fs::copy_options::overwrite_existing);
    }
    FileUtil::writeFile(patch / "fingerprint.json", text);
    FileUtil::writeFile(contentDirectory / "fingerprint.json", text);
    Debugger::print("content", "new content patch %s (version %s), %zu file(s) in %s", sha.c_str(), version.c_str(),
                    patchFiles.size(), patch.string().c_str());
    for (const auto &file : patchFiles) Debugger::print("content", "  %s", file.c_str());
    return 0;
}
} // namespace ContentPatchBuilder
} // namespace server
