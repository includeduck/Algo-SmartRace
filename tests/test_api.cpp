#include "TestFramework.h"

#include "../src/api/SmartRaceApi.h"

#include <cmath>

// API robustness (impl plan 4.12 "API"): every call is safe before init /
// with nothing loaded, illegal human commands are rejected with a reason,
// and race snapshots stay consistent across steps.

namespace
{
    bool HasText(const char* s)
    {
        return s != nullptr && s[0] != '\0';
    }

    const RacerView* Human(const RaceSnapshot& snap)
    {
        for (int i = 0; i < snap.racers.count; ++i)
        {
            if (snap.racers[i].isHuman) return &snap.racers[i];
        }
        return nullptr;
    }
}

TEST(Api_BeforeInit_EveryCallIsSafe)
{
    ApiShutdown();

    CHECK(ApiLoadCircuit(0) != ApiStatus::Ok);
    CHECK(ApiRaceStart() != ApiStatus::Ok);
    ApiRaceStep(10);
    CHECK_EQ(ApiRaceSnapshot().racers.count, 0);
    CHECK(ApiChampionshipNextRace() != ApiStatus::Ok);
    CHECK_EQ(ApiCircuitCount(), 0);
    CHECK(ApiChampionshipStart() == ApiStatus::NotInitialized);
}

TEST(Api_NothingLoaded_EveryCallIsSafe)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);

    CHECK_EQ(ApiGetGraph().vertices.count, 0);
    CHECK_EQ(ApiGetTrackInfo().vertexCount, 0);
    CHECK(!ApiValidateTrack().passed);

    CHECK(ApiRaceStart() == ApiStatus::NotLoaded);
    CHECK(HasText(ApiLastError()));
    ApiRaceStep(5);
    CHECK_EQ(ApiRaceSnapshot().racers.count, 0);
    CHECK(!ApiRaceOver() || ApiRaceSnapshot().racers.count == 0);
    CHECK_EQ(ApiRaceResults().rows.count, 0);
    CHECK_EQ(ApiRaceLogSince(-1).lines.count, 0);

    CHECK(ApiCmdChooseEdge(0) == ApiStatus::NotLoaded);
    CHECK(ApiCmdNitro() == ApiStatus::NotLoaded);
    CHECK(ApiCmdRefuel(10) == ApiStatus::NotLoaded);
    CHECK(ApiCmdAutopilot(false) == ApiStatus::NotLoaded);
    CHECK(ApiCmdPreset(Preset::Speed) == ApiStatus::NotLoaded);
    CHECK_EQ(ApiRecommendedRefuel(), 0);

    CHECK(!ApiRunBfs(0, 1).found);
    CHECK_EQ(ApiRunDfsComponents().componentCount, 0);
    CHECK(!ApiRunDfsCycles().hasCycle);
    CHECK_EQ(ApiRunDeadEnds().deadEndVertices.count, 0);
    CHECK(!ApiPlanLap(Preset::Balanced, RouteAlgo::Dijkstra).found);
    CHECK_EQ(ApiCompareRoutes(Preset::Balanced).candidates.count, 0);
    CHECK_EQ(ApiPlanPickups(Preset::Balanced, 150).items.count, 0);
    CHECK(!ApiPlanRefuel(Preset::Balanced).feasible);

    CHECK_EQ(ApiStandings(SortMethod::Merge).rows.count, 0);
    CHECK_EQ(ApiTopK(3).rows.count, 0);
    CHECK_EQ(ApiRaceHistory(0).rows.count, 0);
    ApiTieBreakReason(0, 1);   // no championship: must simply not crash
    CHECK(!ApiChampionshipGetStatus().active);

    CHECK(ApiLabRun(LabAlgo::Dijkstra, ApiLabDefaultInput()) == ApiStatus::NotLoaded);
    CHECK_EQ(ApiLabStepCount(), 0);
    CHECK_EQ(ApiLabFrame(7).step, -1);
    CHECK_EQ(ApiLabEvents().count, 0);
    CHECK(ApiLabSaveGenerated("build/never.trk") == ApiStatus::NotLoaded);

    ApiShutdown();
}

TEST(Api_BadIdsAndPaths_ReturnErrors)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    CHECK(ApiLoadCircuit(-1) != ApiStatus::Ok);
    CHECK(ApiLoadCircuit(99) != ApiStatus::Ok);
    CHECK(ApiLoadCircuitFile("data/circuits/does_not_exist.trk") == ApiStatus::IoError);
    CHECK(HasText(ApiLastError()));
    CHECK(ApiCircuitName(99)[0] == '\0');

    CHECK(ApiLoadCircuit(0) == ApiStatus::Ok);
    int n = ApiGetGraph().vertices.count;
    ReachabilityView r = ApiRunBfs(-5, n + 100);
    CHECK(!r.found);
    LabInput input = ApiLabDefaultInput();
    input.source = n + 3;
    CHECK(ApiLabRun(LabAlgo::Bfs, input) == ApiStatus::InvalidId);
    CHECK_EQ(ApiRaceHistory(-1).rows.count, 0);
    CHECK_EQ(ApiRaceHistory(50).rows.count, 0);
    ApiShutdown();
}

TEST(Api_IllegalHumanCommands_RejectedWithReason)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    CHECK(ApiLoadCircuit(0) == ApiStatus::Ok);
    CHECK(ApiRaceStart() == ApiStatus::Ok);

    // Autopilot is on at the start: manual moves are refused.
    CHECK(ApiCmdChooseEdge(0) == ApiStatus::Illegal);
    CHECK(HasText(ApiLastError()));

    CHECK(ApiCmdAutopilot(false) == ApiStatus::Ok);
    CHECK(ApiCmdChooseEdge(99999) == ApiStatus::Illegal);
    CHECK(ApiCmdChooseEdge(-1) == ApiStatus::Illegal);

    // The Start is not a fuel station.
    CHECK(ApiCmdRefuel(100) == ApiStatus::Illegal);
    CHECK(HasText(ApiLastError()));

    // One nitro charge at the start: the first use works, a second while
    // active is refused.
    CHECK(ApiCmdNitro() == ApiStatus::Ok);
    CHECK(ApiCmdNitro() == ApiStatus::Illegal);
    CHECK(HasText(ApiLastError()));

    CHECK(ApiCmdPreset(Preset::Safety) == ApiStatus::Ok);
    ApiShutdown();
}

TEST(Api_Snapshots_StayConsistentAcrossSteps)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    CHECK(ApiLoadCircuit(1) == ApiStatus::Ok);
    CHECK(ApiRaceStart() == ApiStatus::Ok);
    int laps = ApiGetTrackInfo().laps;
    int racers = ApiRaceSnapshot().racers.count;
    CHECK(racers >= 4);

    int lastTime = ApiRaceSnapshot().timeMs;
    int steps = 0;
    while (!ApiRaceOver() && steps < 3000)
    {
        ApiRaceStep(15);
        ++steps;
        RaceSnapshot snap = ApiRaceSnapshot();
        CHECK_EQ(snap.racers.count, racers);
        CHECK(snap.timeMs == lastTime + 15 * 20 || snap.raceOver);
        lastTime = snap.timeMs;

        int positionSeen = 0;
        for (int i = 0; i < snap.racers.count; ++i)
        {
            const RacerView& r = snap.racers[i];
            CHECK(r.fuelTenths >= 0 && r.fuelTenths <= 1000);
            CHECK(r.nitroCharges >= 0 && r.nitroCharges <= 3);
            CHECK(r.lap >= 1 && r.lap <= laps + 1);
            CHECK(r.livePosition >= 1 && r.livePosition <= racers);
            CHECK(r.x == r.x && r.y == r.y && fabs(r.x) < 1e9 && fabs(r.y) < 1e9);   // finite
            CHECK(r.state != RacerState::OnEdge || (r.edgeProgress >= 0.0 && r.edgeProgress <= 1.0));
            positionSeen |= 1 << (r.livePosition - 1);
        }
        CHECK_EQ(positionSeen, (1 << racers) - 1);   // live positions are a permutation
    }
    CHECK(ApiRaceOver());
    CHECK_EQ(ApiRaceResults().rows.count, racers);
    ApiShutdown();
}

TEST(Api_Settings_ValidatedAndStartNitroApplied)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    Settings original = ApiGetSettings();

    Settings bad = original;
    bad.laps = 4;
    CHECK(ApiSetSettings(bad) == ApiStatus::InvalidArgument);
    bad = original;
    bad.opponentCount = 2;
    CHECK(ApiSetSettings(bad) == ApiStatus::InvalidArgument);
    bad = original;
    bad.opponentCount = 10;
    CHECK(ApiSetSettings(bad) == ApiStatus::InvalidArgument);
    bad = original;
    bad.startNitroCharges = 4;
    CHECK(ApiSetSettings(bad) == ApiStatus::InvalidArgument);
    CHECK(HasText(ApiLastError()));
    CHECK_EQ(ApiGetSettings().laps, original.laps);
    CHECK_EQ(ApiGetSettings().opponentCount, original.opponentCount);

    Settings good = original;
    good.startNitroCharges = 3;
    good.opponentCount = 5;
    CHECK(ApiSetSettings(good) == ApiStatus::Ok);
    CHECK(ApiLoadCircuit(2) == ApiStatus::Ok);
    CHECK(ApiRaceStart() == ApiStatus::Ok);
    RaceSnapshot snap = ApiRaceSnapshot();
    CHECK_EQ(snap.racers.count, 6);
    for (int i = 0; i < snap.racers.count; ++i) CHECK_EQ(snap.racers[i].nitroCharges, 3);

    good.startNitroCharges = 0;
    CHECK(ApiSetSettings(good) == ApiStatus::Ok);
    CHECK(ApiRaceStart() == ApiStatus::Ok);
    const RacerView* h = Human(ApiRaceSnapshot());
    CHECK(h != nullptr && h->nitroCharges == 0);
    CHECK(ApiCmdNitro() == ApiStatus::Illegal);   // no charges

    CHECK(ApiSetSettings(original) == ApiStatus::Ok);
    ApiShutdown();
}

TEST(Api_SameSettingsTwice_SameResults)
{
    int times[2][10];
    int count[2] = { 0, 0 };
    for (int run = 0; run < 2; ++run)
    {
        CHECK(ApiInit("data") == ApiStatus::Ok);
        CHECK(ApiLoadCircuit(2) == ApiStatus::Ok);
        CHECK(ApiRaceStart() == ApiStatus::Ok);
        while (!ApiRaceOver()) ApiRaceStep(50);
        ResultsView res = ApiRaceResults();
        count[run] = res.rows.count;
        for (int i = 0; i < res.rows.count && i < 10; ++i) times[run][i] = res.rows[i].finishTimeMs * 16 + res.rows[i].racerId;
        ApiShutdown();
    }
    CHECK_EQ(count[0], count[1]);
    for (int i = 0; i < count[0] && i < 10; ++i) CHECK_EQ(times[0][i], times[1][i]);
}
