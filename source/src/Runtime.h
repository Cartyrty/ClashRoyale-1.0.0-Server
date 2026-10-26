#pragma once
#include <string>
// Process start-up: Objective-C image registration (classes, categories, selectors, +load) and the C++ static
// initializers in __mod_init_func, in dyld's order (libobjc's map_images / load_images run before initializers).
namespace runtime {
void setBundlePath(const std::string &path);
const std::string &bundlePath();
void initialize();
} // namespace runtime
