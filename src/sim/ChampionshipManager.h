#pragma once

#include "../model/Records.h"
#include "../algo/AlgoStats.h"
#include "../algo/Comparators.h"
#include "../core/ds/DynArray.h"

namespace sim
{
    enum class SortMethod
    {
        Merge,
        Quick,
        Heap,
    };

    // Accumulates race results into championship standings (Project.pdf
    // sections 2.2 and 4.6) and ranks them. Every ranking goes through the
    // single comparator in algo/Comparators (StandingOrder), whichever of
    // the three sorts or the top-k heap is asked for, so they all agree.
    //
    // Scoring rules applied by ApplyRace:
    //   totalPoints          += the race's pointsAwarded (position points + fastest-lap bonus;
    //                           unfinished racers score 0, see RaceEngine::GetResults)
    //   wins                 += 1 only for a FINISHED 1st place
    //   sumOfFinishPositions += the classified position, DNF/time-limit included
    //   bestFastestLapMs      = min over races (INT_MAX until a lap is completed)
    class ChampionshipManager
    {
    public:
        ChampionshipManager();

        ChampionshipManager(const ChampionshipManager&) = delete;
        ChampionshipManager& operator=(const ChampionshipManager&) = delete;

        // Starts a fresh championship for racers with ids 0..racerCount-1
        // (the race engine assigns ids the same way every race, so one
        // racer keeps one id for the whole championship).
        void Reset(int racerCount);

        void ApplyRace(const DynArray<model::RaceResultRow>& results);

        int RacesCompleted() const
        {
            return m_racesCompleted;
        }

        int EntryCount() const
        {
            return m_entries.Size();
        }

        // The raw, UNSORTED entries (indexed by racer id) -- what the
        // Algorithm Lab hands a sort to watch it rank them.
        const DynArray<model::ChampionshipEntry>& Entries() const
        {
            return m_entries;
        }

        bool IsComplete() const;   // all scheduled races run
        int ChampionId() const;      // -1 until IsComplete()

        // A sorted copy of the standings using `method`. `stats` (optional)
        // receives that sort's comparison/swap counts for the dashboard.
        void Standings(SortMethod method, DynArray<model::ChampionshipEntry>* out,
            algo::AlgoStats* stats = nullptr) const;

        // True when Merge, Quick and Heap Sort produce the same order.
        bool SelfCheck() const;

        // The best k entries via the size-k heap (Challenge 3), best first.
        int TopK(int k, DynArray<model::ChampionshipEntry>* out, algo::AlgoStats* stats = nullptr) const;

        // Which criterion orders racers a and b (both must be valid ids).
        algo::TieBreakReason TieBreak(int racerA, int racerB) const;

        // Results of race `raceIndex` (0-based) as a borrowed pointer into
        // the manager's own storage; returns the row count (0 if no such
        // race). Valid until the next ApplyRace/Reset.
        int RaceHistory(int raceIndex, const model::RaceResultRow** outRows) const;

    private:
        DynArray<model::ChampionshipEntry> m_entries;   // indexed by racer id
        DynArray<model::RaceResultRow> m_history;        // every race's rows, back to back
        DynArray<int> m_raceStart;                         // offset of race i's first row in m_history
        int m_racesCompleted;
    };
}
