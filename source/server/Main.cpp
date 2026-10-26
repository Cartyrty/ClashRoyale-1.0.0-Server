// ClashRoyaleServer: a v1.0.0 game server that runs the original library's own logic (messages, LogicGameMode,
// commands, checksums, battles) under the HLE core, the same approach as the libg.so servers for Android builds.
// usage: ClashRoyaleServer [--host ADDRESS] [--port N] [--content-port N] [--content-url URL] [--data DIR]
//                          [--library PATH] [--res DIR] [--shop-period SECONDS]
//        ClashRoyaleServer --build-content <master assets folder> [--data DIR]   (makes a content patch, then exits)
#include "Cpu.h"
#include "Hle.h"
#include "Loader.h"
#include "Memory.h"
#include "Runtime.h"
#include "game/ServerApplication.h"
#include "game/content/ContentPatchBuilder.h"
#include "game/utils/TimeUtil.h"
#include "libc/Files.h"
#include "library/Library.h"
#include "logic/data/LogicResources.h"
#include "titan/logic/debug/Debugger.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {
namespace fs = std::filesystem;

constexpr std::size_t kStackSize = 8 * 1024 * 1024;
constexpr int kMinShopPeriodSeconds = 10;

struct CommandLine {
    server::ServerConfig config;
    std::string library;
    std::string resourceOverride;
    std::string buildContent;
};

CommandLine parseCommandLine(int argc, char **argv) {
    CommandLine commandLine;
    auto &config = commandLine.config;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        auto value = [&] {
            if (i + 1 >= argc) server::Debugger::error("%s needs a value", option.c_str());
            return std::string(argv[++i]);
        };
        if (option == "--port")
            config.port = std::atoi(value().c_str());
        else if (option == "--host")
            config.host = value(); // 0.0.0.0 accepts other machines
        else if (option == "--content-port")
            config.contentPort = std::atoi(value().c_str()); // 0: no content updates
        else if (option == "--udp-port")
            config.udpPort = std::atoi(value().c_str()); // 0: battles over TCP only
        else if (option == "--udp-host")
            config.udpHost = value();
        else if (option == "--content-url")
            config.contentUrl = value();
        else if (option == "--build-content")
            commandLine.buildContent = value();
        else if (option == "--data")
            config.dataDirectory = value();
        else if (option == "--library")
            commandLine.library = value();
        else if (option == "--res")
            commandLine.resourceOverride = value(); // a separate copy of res/ (for example modified CSVs)
        else if (option == "--shop-period")
            server::TimeUtil::setShopPeriodSeconds(std::max(kMinShopPeriodSeconds, std::atoi(value().c_str())));
        else
            server::Debugger::error("unknown option %s", option.c_str());
    }
    return commandLine;
}

// Defaults: the project's inputs/original library, and server-data next to the ClashRoyaleHLE project.
void findDefaults(CommandLine &commandLine, const fs::path &executableDirectory) {
    auto &config = commandLine.config;
    for (fs::path directory = executableDirectory; !directory.empty() && directory != directory.root_path();
         directory = directory.parent_path()) {
        if (commandLine.library.empty() && fs::exists(directory / "inputs/original/library"))
            commandLine.library = (directory / "inputs/original/library").string();
        if (config.dataDirectory.empty() && fs::exists(directory / "CMakeLists.txt") &&
            fs::exists(directory / "server"))
            config.dataDirectory = (directory / "server-data").string();
    }
    if (commandLine.library.empty()) server::Debugger::error("the original library was not found (use --library)");
    if (config.dataDirectory.empty()) config.dataDirectory = (executableDirectory / "server-data").string();
    fs::create_directories(config.dataDirectory);
    const auto resources = commandLine.resourceOverride.empty() ? fs::path(commandLine.library).parent_path() / "res"
                                                                : fs::absolute(commandLine.resourceOverride);
    config.resourceDirectory = resources.string();
}

// The library under the HLE core, as the client starts it, with every data table loaded.
void startLibrary(const CommandLine &commandLine) {
    const auto start = std::chrono::steady_clock::now();
    mem::init();
    cpu::initProcess();
    hle::createSpecialStubs();
    runtime::setBundlePath(fs::path(commandLine.config.resourceDirectory).parent_path().string());
    files::setSandboxRoot((fs::path(commandLine.config.dataDirectory) / "sandbox").string());
    loader::load(commandLine.library);
    cpu::attachThread(mem::allocStack(kStackSize));
    runtime::initialize();
    server::LogicResources::loadDataTables();
    server::Debugger::print(
        "server", "library ready in %lld ms",
        (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
            .count());
}

// HLE_DUMPSTR=address,address,...: prints library String globals (a reverse-engineering aid).
void dumpStrings() {
    const char *addresses = std::getenv("HLE_DUMPSTR");
    if (!addresses) return;
    const std::string list = addresses;
    for (std::size_t at = 0; at < list.size();) {
        auto end = list.find(',', at);
        if (end == std::string::npos) end = list.size();
        const auto address = std::strtoull(list.substr(at, end - at).c_str(), nullptr, 16);
        server::Debugger::print("dump", "%llx = \"%s\"", (unsigned long long)address,
                                library::readString(address).c_str());
        at = end + 1;
    }
}
} // namespace

int main(int argc, char **argv) {
#ifdef _WIN32
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    auto commandLine = parseCommandLine(argc, argv);
    findDefaults(commandLine, fs::absolute(fs::path(argv[0]).parent_path()));
    if (!commandLine.buildContent.empty())
        return server::ContentPatchBuilder::build(
            fs::absolute(commandLine.buildContent).string(),
            (fs::path(commandLine.library).parent_path() / "res" / "fingerprint.json").string(),
            commandLine.config.dataDirectory);
    startLibrary(commandLine);
    dumpStrings();
    // Ctrl+C / SIGTERM: a clean stop (players saved, database closed) instead of the process being killed.
    std::signal(SIGINT, [](int) { server::ServerApplication::requestStop(); });
    std::signal(SIGTERM, [](int) { server::ServerApplication::requestStop(); });
    const int exitCode = server::ServerApplication::run(commandLine.config);
    server::Debugger::print("server", "stopped");
    return exitCode;
}
