#pragma once
// Content updates, the game's own flow: the client's LoginMessage carries the master SHA of its fingerprint.json. A
// different SHA gets LoginFailedMessage error 7 with the server's fingerprint.json and the content URL; the client
// (InitState::startContentUpdate -> ResourceManager::checkForUpdates) downloads every file whose SHA differs from
// <content URL>/<master sha>/<file> (HTTPClient checks each file's SHA1) into its updated folder, then loads with them.
// At startup the server only loads the last built patch (content/fingerprint.json, see ContentPatchBuilder), or the
// game's own fingerprint when none was built; ContentHttpServer serves the files.
#include <string>

namespace server {
class ContentManager {
public:
    ContentManager(const std::string &resourceDirectory, const std::string &dataDirectory,
                   const std::string &urlOverride);

    bool isEnabled() const { return !sha_.empty(); }
    const std::string &getSha() const { return sha_; }
    const std::string &getFingerprintJSON() const { return fingerprint_; }
    bool isUpToDate(const std::string &clientSha) const { return clientSha == sha_; }
    void setPort(int port) { port_ = port; }
    // The URL the client downloads from: the override, or http://<address the client reached>:<content port>.
    std::string getContentUrl(const std::string &serverAddress) const;

private:
    std::string urlOverride_;
    std::string sha_;
    std::string fingerprint_;
    int port_ = 0;
};
} // namespace server
