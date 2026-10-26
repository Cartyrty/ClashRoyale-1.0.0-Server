#pragma once
// Loads the app's 64-bit Mach-O at its preferred address (no slide, so no rebasing) and binds every import (eager,
// lazy and weak binds from LC_DYLD_INFO, as dyld-360 ImageLoaderMachOCompressed does) to HLE stubs and data.
#include <cstdint>
#include <string>
#include <vector>

namespace loader {
struct Section {
    std::string segment, name;
    std::uint64_t address = 0, size = 0;
};
void load(const std::string &path);
std::uint64_t entryPoint();                 // LC_MAIN
std::vector<std::uint64_t> initializers();  // __mod_init_func
const Section *section(const std::string &segment, const std::string &name);
std::uint64_t symbol(const std::string &name); // defined symbol address, 0 if absent
std::string describe(std::uint64_t address);   // "symbol+0x12 (0x1000abcd)"
std::uint64_t imageBase();
std::uint64_t imageEnd();
} // namespace loader

namespace hle {
const char *stubName(std::uint64_t address);
}
