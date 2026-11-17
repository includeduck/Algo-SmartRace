#pragma once

#include "../model/Records.h"

// The ONE total order each ranking in this project uses (Project.pdf
// sections 4.2 and 4.6). Every sort and top-k call is handed one of the
// policy structs below, so Merge Sort, Quick Sort, Heap Sort and both
// top-k variants cannot disagree.
namespace algo
{
    // Which championship criterion decided the order of two entries; used
    // by the dashboard's "why is A above B" explanation.
    enum class TieBreakReason
    {
        Points,
        Wins,
        SumOfFinishPositions,
        BestFastestLap,
        RacerId,
        Identical,   // same racer compared with itself
    };

    // Championship standings (section 4.6): higher points; then more wins;
    // then lower sum of finishing positions; then lower best fastest lap;
    // then lower racer id. Returns < 0 if `a` ranks before `b`. Total: two
    // different racers never compare equal.
    int CompareStanding(const model::ChampionshipEntry& a, const model::ChampionshipEntry& b);

    TieBreakReason StandingTieBreakReason(const model::ChampionshipEntry& a, const model::ChampionshipEntry& b);

    // Single-race finish order (sections 4.2.7-8): finished racers first,
    // by finish time then lower id; unfinished racers after, by laps
    // completed, then checkpoints completed in the current lap, then lower
    // elapsed active-driving time, then lower id.
    int CompareFinish(const model::Racer& a, const model::Racer& b);

    struct StandingOrder
    {
        static int Compare(const model::ChampionshipEntry& a, const model::ChampionshipEntry& b)
        {
            return CompareStanding(a, b);
        }

        static int Key(const model::ChampionshipEntry& e)
        {
            return e.racerId;
        }
    };

    // Racers are large (they carry their planned path), so the race engine
    // sorts pointers to them rather than copies.
    struct FinishOrderPtr
    {
        static int Compare(const model::Racer* const& a, const model::Racer* const& b)
        {
            return CompareFinish(*a, *b);
        }

        static int Key(const model::Racer* const& r)
        {
            return r->id;
        }
    };
}
