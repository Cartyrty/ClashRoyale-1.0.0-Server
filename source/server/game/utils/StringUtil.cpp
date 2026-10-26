#include "game/utils/StringUtil.h"

#include <cctype>

namespace server {
namespace StringUtil {
namespace {
constexpr unsigned char kFirstPrintable = 0x20;
constexpr unsigned char kDelete = 0x7f;

// A UTF-8 continuation byte (10xxxxxx) does not start a character.
bool startsCharacter(unsigned char byte) { return (byte & 0xC0) != 0x80; }
} // namespace

std::size_t characterCount(const std::string &text) {
    std::size_t count = 0;
    for (unsigned char byte : text)
        if (startsCharacter(byte)) ++count;
    return count;
}

std::string truncateCharacters(const std::string &text, std::size_t maxCharacters) {
    std::size_t count = 0;
    std::size_t end = 0;
    for (; end < text.size(); ++end) {
        if (!startsCharacter(static_cast<unsigned char>(text[end]))) continue;
        if (count == maxCharacters) break;
        ++count;
    }
    return text.substr(0, end);
}

std::string toLower(std::string text) {
    for (auto &c : text) c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string trimSpaces(const std::string &text) {
    const auto first = text.find_first_not_of(' ');
    const auto last = text.find_last_not_of(' ');
    return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
}

bool hasControlCharacters(const std::string &text) {
    for (unsigned char c : text)
        if (c < kFirstPrintable || c == kDelete) return true;
    return false;
}

bool isAllowedInput(const std::string &text, std::size_t maxCharacters) {
    return characterCount(text) <= maxCharacters && text.find("  ") == std::string::npos && !hasControlCharacters(text);
}
} // namespace StringUtil
} // namespace server
