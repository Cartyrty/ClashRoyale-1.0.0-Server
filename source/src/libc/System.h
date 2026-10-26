#pragma once
#include <cstdint>
#include <functional>
// Threads, blocks and Grand Central Dispatch.
namespace sys {
// Runs fn on a new guest thread (its own JIT and stack).
void spawnGuestThread(const char *name, std::function<void()> fn);
// Blocks (Block_layout: isa +0, flags +8, reserved +12, invoke +16, descriptor +24).
std::uint64_t blockCopy(std::uint64_t block);
void blockRelease(std::uint64_t block);
std::uint64_t callBlock(std::uint64_t block, std::initializer_list<std::uint64_t> args = {});
// The main dispatch queue: tasks posted to it run when the main run loop drains it.
void drainMainQueue();
bool isMainThread();
void markMainThread();
} // namespace sys
