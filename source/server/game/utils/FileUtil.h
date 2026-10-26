#pragma once
// Whole-file reads and writes, and JSON files.
#include "game/utils/Json.h"

#include <filesystem>
#include <string>
#include <vector>

namespace server {
namespace FileUtil {
std::string readFile(const std::filesystem::path &path); // "" when missing
std::vector<std::uint8_t> readBytes(const std::filesystem::path &path);
void writeFile(const std::filesystem::path &path, const std::string &data); // creates the folders
// A JSON file's value; null when the file is missing or not JSON (or, with requireObject, not an object).
json::Value readJson(const std::filesystem::path &path, bool requireObject = true);
} // namespace FileUtil
} // namespace server
