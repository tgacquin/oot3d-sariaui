// Host adapter for the upstream generator. No emulated hardware services.
#pragma once
#include "s_types.h"
#include <chrono>
#include <cstdio>
#include <string>
using FS_Archive = u64;
using Result = s32;
using Handle = u32;
constexpr u32 KEY_LEFT = 1 << 5, KEY_RIGHT = 1 << 4;
inline u64 svcGetSystemTick() {
    return std::chrono::steady_clock::now().time_since_epoch().count();
}
inline void svcOutputDebugString(const char* text, std::size_t size) {
    std::fwrite(text, 1, size, stderr);
}
