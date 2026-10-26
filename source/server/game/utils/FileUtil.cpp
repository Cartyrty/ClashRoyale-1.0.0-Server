#include "game/utils/FileUtil.h"

#include <fstream>
#include <iterator>

namespace server {
namespace FileUtil {
namespace fs = std::filesystem;

std::string readFile(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

std::vector<std::uint8_t> readBytes(const fs::path &path) {
    const auto data = readFile(path);
    return std::vector<std::uint8_t>(data.begin(), data.end());
}

void writeFile(const fs::path &path, const std::string &data) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(data.data(), std::streamsize(data.size()));
}

json::Value readJson(const fs::path &path, bool requireObject) {
    if (!fs::exists(path)) return {};
    bool ok = false;
    auto value = json::Value::parse(readFile(path), &ok);
    if (!ok || (requireObject && !value.isObject())) return {};
    return value;
}
} // namespace FileUtil
} // namespace server
