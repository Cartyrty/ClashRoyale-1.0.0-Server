#pragma once
// Text rules shared by names, clan settings and chat. Lengths count characters (UTF-8 code points), as the client's
// input fields do.
#include <cstddef>
#include <string>

namespace server {
namespace StringUtil {
std::size_t characterCount(const std::string &text);
std::string truncateCharacters(const std::string &text, std::size_t maxCharacters);
std::string toLower(std::string text);
std::string trimSpaces(const std::string &text); // spaces at either end
bool hasControlCharacters(const std::string &text);
// KeyInput (AllianceSettings::createInputField): at most maxCharacters, no control characters, no double spaces.
bool isAllowedInput(const std::string &text, std::size_t maxCharacters);
} // namespace StringUtil
} // namespace server
