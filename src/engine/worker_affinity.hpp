#pragma once
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace engine::detail {
// Best effort: distinct allowed logical CPUs for input and rendering. Respect
// the process affinity mask and gracefully handle one-CPU environments.
inline void worker_affinity(bool input) noexcept {
#ifdef _WIN32
    DWORD_PTR allowed{}, system{};
    if (!GetProcessAffinityMask(GetCurrentProcess(),&allowed,&system)) return;
    DWORD_PTR last{}, previous{};
    for (unsigned bit=0;bit<sizeof(DWORD_PTR)*8;++bit) {
        const DWORD_PTR mask=DWORD_PTR{1}<<bit;
        if (allowed&mask) { previous=last; last=mask; }
    }
    if (previous) SetThreadAffinityMask(GetCurrentThread(),input?last:previous);
#else
    (void)input;
#endif
}
}
