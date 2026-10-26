#pragma once
// Build-ContentPatch.bat (ClashRoyaleServer --build-content <master assets>), as AssetsManager / content.js build a
// patch: every file of the master assets folder is hashed (SHA1, as served) and compared with the game's own
// fingerprint.json. Changes since the last patch make a new fingerprint (the version's last part + 1, a master SHA
// from the file list and the time) and a patch folder content/update/<sha>/ with the changed files and
// fingerprint.json; content/fingerprint.json is what the server loads at its next start. Files the game does not
// fingerprint at the top of res (settings.csv, version.number) are not content. 1.0.0 ships its CSVs uncompressed and
// fingerprints them that way, so files are served as they are.
#include <string>

namespace server {
namespace ContentPatchBuilder {
// The process exit code.
int build(const std::string &masterDirectory, const std::string &gameFingerprintPath, const std::string &dataDirectory);
} // namespace ContentPatchBuilder
} // namespace server
