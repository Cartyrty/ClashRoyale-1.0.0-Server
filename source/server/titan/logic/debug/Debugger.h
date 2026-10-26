#pragma once
// Debugger as on the client (titan/logic/debug/Debugger): the server's log. print writes to a channel ("[channel]
// message"; channels are silenced with HLE_QUIET=channel,...), warning flags something wrong that the server
// survives, and error stops the server. Messages are printf formats.
namespace server {
class Debugger {
public:
    static void print(const char *channel, const char *format, ...);
    static void warning(const char *format, ...);
    [[noreturn]] static void error(const char *format, ...);
    // An error when the condition does not hold.
    static void doAssert(bool condition, const char *message);
};
} // namespace server
