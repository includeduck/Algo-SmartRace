#pragma once
#include "api/SmartRaceApi.h"

namespace LabSource
{
    inline const char* Name(LabDataSource source)
    {
        switch (source)
        {
            case LabDataSource::LoadedCircuit: return "LOADED CIRCUIT";
            case LabDataSource::LiveRace: return "LIVE RACE";
            case LabDataSource::BriefExample: return "BRIEF EXAMPLE";
            case LabDataSource::BriefExampleFallback: return "EXAMPLE FALLBACK";
            case LabDataSource::ChampionshipStandings: return "CHAMPIONSHIP RECORDS";
            case LabDataSource::RandomKeys: return "GENERATED ARRAY KEYS";
            case LabDataSource::GeneratedTrack: return "GENERATED TRACK";
            case LabDataSource::CircuitEvents: return "CIRCUIT EVENT SCRIPT";
            case LabDataSource::None: return "NO RUN";
        }
        return "UNKNOWN SOURCE";
    }
}
