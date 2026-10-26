// Host replacements for app functions that depend on runtime pieces not worth emulating (GNU iostreams) or on
// online services the emulator does not provide (Facebook).
#include "Hle.h"
#include "Log.h"
#include "Memory.h"
#include <cstdlib>
#include <string>

namespace {
// titan String: m_length +0, m_bytes +4, characters inline at +8 when m_bytes + 1 <= 8, else a pointer at +8.
std::string titanString(std::uint64_t s) {
    const auto bytes = mem::r32(s + 4);
    const auto chars = bytes + 1 > 8 ? mem::r64(s + 8) : s + 8;
    return chars ? std::string(reinterpret_cast<const char *>(mem::base() + chars), bytes) : std::string();
}
} // namespace

// LogicStringUtil::strHexToInt(const String &): std::stringstream >> std::hex >> unsigned.
static hle::PatchRegistrar p_strHexToInt("__ZN15LogicStringUtil11strHexToIntERK6String", [] {
    const auto text = titanString(hle::arg(0));
    hle::ret(text.empty() ? 0 : std::uint32_t(std::strtoul(text.c_str(), nullptr, 16)));
});
// FacebookManager::init(String appId, String ...): the Facebook SDK stays uninitialised (offline).
static hle::PatchRegistrar p_facebookInit("__ZN15FacebookManager4initE6StringS0_", [] {
    hlog("patch", "FacebookManager::init skipped (Facebook is offline)");
});
// FacebookConnection::isLogged() const: never logged in.
static hle::PatchRegistrar p_facebookLogged("__ZNK18FacebookConnection8isLoggedEv", [] { hle::ret(0); });
// Input diagnostics (HLE_TOUCHLOG=1): the touch position Stage::touchPressed receives (Touch +52/+56) and the stage
// scale (+292).
static const bool g_touchLog = [] {
    if (std::getenv("HLE_TOUCHLOG"))
        hle::afterLoad([] { hle::hookFunction("__ZN5Stage12touchPressedERK5Touch", [] {
            const auto stage = hle::arg(0), touch = hle::arg(1);
            hlog("touch", "Stage::touchPressed at %.1f,%.1f (stage scale %.4f)", mem::read<float>(touch + 52), mem::read<float>(touch + 56),
                 mem::read<float>(stage + 292));
        }); });
    return true;
}();
