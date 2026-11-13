// Windows-only implementation of Timer::NowMicros(). <windows.h> is
// included ONLY in this .cpp, never in a header, so it can never clash with
// a GUI library's own identifiers (raylib, for instance, defines
// Rectangle/DrawText/CloseWindow, which collide with windows.h macros).
#include "../util/Timer.h"

#include <windows.h>

namespace
{
    double QueryFrequency()
    {
        LARGE_INTEGER freq;
        QueryPerformanceFrequency(&freq);
        return static_cast<double>(freq.QuadPart);
    }
}

double Timer::NowMicros()
{
    static double frequency = QueryFrequency();
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (static_cast<double>(counter.QuadPart) * 1000000.0) / frequency;
}
