#include "Runtime.h"
#include "Cpu.h"
#include "Loader.h"
#include "Log.h"
#include "objc/ObjC.h"
#include "Hle.h"

namespace runtime {
namespace {
std::string g_bundle;
}
void setBundlePath(const std::string &path) { g_bundle = path; }
const std::string &bundlePath() { return g_bundle; }
void initialize() {
    hle::applyPatches();
    objc::loadImage();
    const auto inits = loader::initializers();
    hlog("init", "%zu static initializers", inits.size());
    for (std::size_t i = 0; i < inits.size(); ++i) {
        hlog("init", "[%zu] %s", i, loader::describe(inits[i]).c_str());
        cpu::call(inits[i], {0, 0, 0, 0, 0});
    }
}
} // namespace runtime
