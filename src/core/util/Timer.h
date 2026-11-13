#pragma once

// Timer: wall-clock microseconds for the benchmark harness and API
// counters. The Windows implementation lives in core/platform/Timer_win.cpp
// so <windows.h> never appears in a header (it would clash with a GUI
// library's own typedefs, e.g. raylib's Rectangle/DrawText/CloseWindow).
// <chrono> is STL, so we go straight to QueryPerformanceCounter instead.
class Timer
{
public:
    static double NowMicros();
};
