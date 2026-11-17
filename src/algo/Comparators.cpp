#include "Comparators.h"

namespace algo
{
    TieBreakReason StandingTieBreakReason(const model::ChampionshipEntry& a, const model::ChampionshipEntry& b)
    {
        if (a.totalPoints != b.totalPoints) return TieBreakReason::Points;
        if (a.wins != b.wins) return TieBreakReason::Wins;
        if (a.sumOfFinishPositions != b.sumOfFinishPositions) return TieBreakReason::SumOfFinishPositions;
        if (a.bestFastestLapMs != b.bestFastestLapMs) return TieBreakReason::BestFastestLap;
        if (a.racerId != b.racerId) return TieBreakReason::RacerId;
        return TieBreakReason::Identical;
    }

    int CompareStanding(const model::ChampionshipEntry& a, const model::ChampionshipEntry& b)
    {
        switch (StandingTieBreakReason(a, b))
        {
            case TieBreakReason::Points:
                return (a.totalPoints > b.totalPoints) ? -1 : 1;
            case TieBreakReason::Wins:
                return (a.wins > b.wins) ? -1 : 1;
            case TieBreakReason::SumOfFinishPositions:
                return (a.sumOfFinishPositions < b.sumOfFinishPositions) ? -1 : 1;
            case TieBreakReason::BestFastestLap:
                return (a.bestFastestLapMs < b.bestFastestLapMs) ? -1 : 1;
            case TieBreakReason::RacerId:
                return (a.racerId < b.racerId) ? -1 : 1;
            case TieBreakReason::Identical:
            default:
                return 0;
        }
    }

    int CompareFinish(const model::Racer& a, const model::Racer& b)
    {
        bool aFinished = (a.state == model::RacerState::Finished);
        bool bFinished = (b.state == model::RacerState::Finished);

        if (aFinished && bFinished)
        {
            if (a.finishTimeMs != b.finishTimeMs) return (a.finishTimeMs < b.finishTimeMs) ? -1 : 1;
        }
        else if (aFinished != bFinished)
        {
            return aFinished ? -1 : 1;
        }
        else
        {
            int aLaps = a.lap - 1;   // the lap currently being driven does not count yet
            int bLaps = b.lap - 1;
            if (aLaps != bLaps) return (aLaps > bLaps) ? -1 : 1;

            if (a.checkpointsCompletedThisLap != b.checkpointsCompletedThisLap)
            {
                return (a.checkpointsCompletedThisLap > b.checkpointsCompletedThisLap) ? -1 : 1;
            }

            if (a.totalTimeMs != b.totalTimeMs) return (a.totalTimeMs < b.totalTimeMs) ? -1 : 1;
        }

        if (a.id != b.id) return (a.id < b.id) ? -1 : 1;
        return 0;
    }
}
