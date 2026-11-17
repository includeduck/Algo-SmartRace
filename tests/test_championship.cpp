#include "TestFramework.h"

#include "../src/sim/ChampionshipManager.h"
#include "../src/model/Rules.h"

using namespace model;
using namespace sim;

namespace
{
    RaceResultRow Row(int racerId, int position, RaceStatus status, int bestLapMs, int points)
    {
        RaceResultRow r;
        r.racerId = racerId;
        r.finishPosition = position;
        r.finishTimeMs = (status == RaceStatus::Finished) ? 100000 + position : -1;
        r.bestLapTimeMs = bestLapMs;
        r.bonusPoints = 0;
        r.pointsAwarded = points;
        r.status = status;
        r.lapsCompleted = 3;
        r.checkpointsCompletedInFinalLap = 0;
        return r;
    }
}

TEST(Championship_AccumulatesPointsWinsPositionsAndBestLap)
{
    ChampionshipManager champ;
    champ.Reset(3);

    DynArray<RaceResultRow> race1;
    race1.Push(Row(2, 1, RaceStatus::Finished, 40000, 26));
    race1.Push(Row(0, 2, RaceStatus::Finished, 42000, 18));
    race1.Push(Row(1, 3, RaceStatus::Dnf, 0x7fffffff, 0));
    champ.ApplyRace(race1);

    DynArray<RaceResultRow> race2;
    race2.Push(Row(0, 1, RaceStatus::Finished, 39000, 26));
    race2.Push(Row(2, 2, RaceStatus::Finished, 41000, 18));
    race2.Push(Row(1, 3, RaceStatus::Finished, 45000, 15));
    champ.ApplyRace(race2);

    CHECK_EQ(champ.RacesCompleted(), 2);
    CHECK(!champ.IsComplete());
    CHECK_EQ(champ.ChampionId(), -1);

    DynArray<ChampionshipEntry> standings;
    champ.Standings(SortMethod::Merge, &standings);

    // Racers 0 and 2 are tied on 44 points and 1 win each; sum of
    // positions is also 3 each; racer 0's best lap (39000) beats racer 2's
    // (40000), so racer 0 leads on the fastest-lap criterion.
    CHECK_EQ(standings[0].racerId, 0);
    CHECK_EQ(standings[0].totalPoints, 44);
    CHECK_EQ(standings[0].wins, 1);
    CHECK_EQ(standings[0].sumOfFinishPositions, 3);
    CHECK_EQ(standings[0].bestFastestLapMs, 39000);
    CHECK_EQ(standings[1].racerId, 2);
    CHECK_EQ(standings[2].racerId, 1);
    CHECK_EQ(standings[2].sumOfFinishPositions, 6);   // the DNF's classified 3rd place counts

    CHECK(champ.TieBreak(0, 2) == algo::TieBreakReason::BestFastestLap);
}

TEST(Championship_UnfinishedFirstPlaceIsNotAWin)
{
    ChampionshipManager champ;
    champ.Reset(2);

    // Nobody finished: P1 is only "classified", it must not count as a win.
    DynArray<RaceResultRow> race;
    race.Push(Row(1, 1, RaceStatus::TimeLimit, 0x7fffffff, 0));
    race.Push(Row(0, 2, RaceStatus::Dnf, 0x7fffffff, 0));
    champ.ApplyRace(race);

    DynArray<ChampionshipEntry> standings;
    champ.Standings(SortMethod::Merge, &standings);
    for (int i = 0; i < standings.Size(); ++i) CHECK_EQ(standings[i].wins, 0);
}

TEST(Championship_ThreeSortsAgree_TopKMatchesPrefix_ChampionDeclared)
{
    ChampionshipManager champ;
    champ.Reset(6);

    for (int race = 0; race < Rules::kChampionshipRaces; ++race)
    {
        DynArray<RaceResultRow> rows;
        for (int pos = 1; pos <= 6; ++pos)
        {
            int racer = (pos + race) % 6;   // rotate the order each race
            int points = Rules::kChampionshipPoints[pos - 1];
            rows.Push(Row(racer, pos, RaceStatus::Finished, 40000 + racer * 10, points));
        }
        champ.ApplyRace(rows);
    }

    CHECK(champ.IsComplete());
    CHECK(champ.SelfCheck());

    DynArray<ChampionshipEntry> merge;
    DynArray<ChampionshipEntry> quick;
    DynArray<ChampionshipEntry> heap;
    champ.Standings(SortMethod::Merge, &merge);
    champ.Standings(SortMethod::Quick, &quick);
    champ.Standings(SortMethod::Heap, &heap);
    for (int i = 0; i < merge.Size(); ++i)
    {
        CHECK_EQ(merge[i].racerId, quick[i].racerId);
        CHECK_EQ(merge[i].racerId, heap[i].racerId);
    }

    DynArray<ChampionshipEntry> top3;
    CHECK_EQ(champ.TopK(3, &top3), 3);
    for (int i = 0; i < 3; ++i) CHECK_EQ(top3[i].racerId, merge[i].racerId);

    CHECK_EQ(champ.ChampionId(), merge[0].racerId);
}

TEST(Championship_RaceHistory_ReturnsEachRacesRows)
{
    ChampionshipManager champ;
    champ.Reset(2);

    DynArray<RaceResultRow> race1;
    race1.Push(Row(0, 1, RaceStatus::Finished, 40000, 25));
    race1.Push(Row(1, 2, RaceStatus::Finished, 41000, 18));
    champ.ApplyRace(race1);

    DynArray<RaceResultRow> race2;
    race2.Push(Row(1, 1, RaceStatus::Finished, 39000, 26));
    race2.Push(Row(0, 2, RaceStatus::Finished, 40000, 18));
    champ.ApplyRace(race2);

    const RaceResultRow* rows = nullptr;
    CHECK_EQ(champ.RaceHistory(1, &rows), 2);
    CHECK(rows != nullptr);
    CHECK_EQ(rows[0].racerId, 1);
    CHECK_EQ(champ.RaceHistory(5, &rows), 0);
    CHECK(rows == nullptr);
}
