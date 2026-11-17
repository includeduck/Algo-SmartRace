#pragma once

#include "api/SmartRaceApi.h"
#include <cstdio>
#include <climits>

namespace Format
{
    inline void Time(char* text, int size, int ms)
    {
        if (ms < 0 || ms == INT_MAX)
        {
            snprintf(text, size, "--:--.---");
            return;
        }
        snprintf(text, size, "%d:%02d.%03d", ms / 60000, (ms / 1000) % 60, ms % 1000);
    }

    inline const char* PresetName(Preset p)
    {
        const char* names[] = { "Balanced", "Speed", "Economy", "Safety" };
        int i = static_cast<int>(p);
        return i >= 0 && i < 4 ? names[i] : "Unknown";
    }

    inline const char* State(RacerState s)
    {
        const char* names[] = { "At junction", "Racing", "Refuelling", "Waiting", "Finished", "DNF" };
        int i = static_cast<int>(s);
        return i >= 0 && i < 6 ? names[i] : "Unknown";
    }
}
