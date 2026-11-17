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

// ---------------------------------------------------------------------- //
// API v1.1 additions (GUI handoff requests H1-H3, H7, H8)
// ---------------------------------------------------------------------- //

namespace
{
    int VertexWithType(const GraphView& g, VertexType type)
    {
        for (int i = 0; i < g.vertices.count; ++i)
        {
            if (g.vertices[i].type == type) return i;
        }
        return -1;
    }

    int CheckpointVertexV(const GraphView& g, int order)
    {
        for (int i = 0; i < g.vertices.count; ++i)
        {
            if (g.vertices[i].checkpointOrder == order) return i;
        }
        return -1;
    }

    // Every road joins the two vertices around it in the path.
    bool EdgesMatchVertices(const GraphView& g, const ArrayView<int>& vertices, const ArrayView<int>& edges)
    {
        if (vertices.count == 0) return edges.count == 0;
        if (edges.count != vertices.count - 1) return false;
        for (int i = 0; i < edges.count; ++i)
        {
            const EdgeView& e = g.edges[edges[i]];
            int a = vertices[i];
            int b = vertices[i + 1];
            bool forward = (e.from == a && e.to == b);
            bool backward = (!e.directed && e.from == b && e.to == a);
            if (!forward && !backward) return false;
        }
        return true;
    }

    double SumCosts(const ArrayView<double>& costs, const ArrayView<int>& edges)
    {
        double sum = 0.0;
        for (int i = 0; i < edges.count; ++i) sum += costs[edges[i]];
        return sum;
    }

    bool Close(double a, double b)
    {
        double scale = fabs(a) > 1.0 ? fabs(a) : 1.0;
        return fabs(a - b) <= 1e-9 * scale;
    }

    void StepUntil(int timeMs)
    {
        while (!ApiRaceOver() && ApiRaceSnapshot().timeMs < timeMs) ApiRaceStep(5);
    }

    bool IsAlternativeLabel(const char* label)
    {
        const char* prefix = "Alternative";
        for (int i = 0; prefix[i] != '\0'; ++i)
        {
            if (label[i] != prefix[i]) return false;
        }
        return true;
    }
}

TEST(ApiV11_PlanRoute_AnyTwoVertices_StructuredAndConsistent)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    CHECK(ApiLoadCircuit(0) == ApiStatus::Ok);
    GraphView g = ApiGetGraph();
    int start = VertexWithType(g, VertexType::Start);
    int cp5 = CheckpointVertexV(g, 5);

    RouteView d = ApiPlanRoute(start, cp5, Preset::Balanced, RouteAlgo::Dijkstra);
    CHECK(d.found);
    CHECK_EQ(d.blockedLegIndex, -1);
    CHECK(d.expanded > 0 && d.relaxed > 0 && d.heapPushes > 0 && d.heapPops > 0);
    CHECK(EdgesMatchVertices(g, d.vertexPath, d.edgePath));
    CHECK_EQ(d.vertexPath[0], start);
    CHECK_EQ(d.vertexPath[d.vertexPath.count - 1], cp5);
    double dijkstraCost = d.totalCost;
    long long dijkstraExpanded = d.expanded;

    // H3: the per-road costs add up to exactly the optimised total.
    ArrayView<double> costs = ApiEdgeCosts(Preset::Balanced);
    CHECK_EQ(costs.count, g.edges.count);
    CHECK(Close(SumCosts(costs, d.edgePath), dijkstraCost));

    RouteView a = ApiPlanRoute(start, cp5, Preset::Balanced, RouteAlgo::AStar);
    CHECK(a.found);
    CHECK(Close(a.totalCost, dijkstraCost));
    CHECK(a.expanded <= dijkstraExpanded);

    RouteView same = ApiPlanRoute(cp5, cp5, Preset::Speed, RouteAlgo::Dijkstra);
    CHECK(same.found);
    CHECK_EQ(same.vertexPath.count, 1);
    CHECK_EQ(same.edgePath.count, 0);
    CHECK(Close(same.totalCost, 0.0));

    RouteView bad = ApiPlanRoute(-1, 9999, Preset::Balanced, RouteAlgo::AStar);
    CHECK(!bad.found);
    CHECK_EQ(bad.blockedLegIndex, 0);
    CHECK(HasText(ApiLastError()));

    // ApiPlanLap now reports counters too.
    RouteView lap = ApiPlanLap(Preset::Balanced, RouteAlgo::Dijkstra);
    CHECK(lap.found && lap.expanded > 0 && lap.heapPops > 0);
    ApiShutdown();
}

TEST(ApiV11_CompareRoutes_EdgePathsAndSourcePresets)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    for (int c = 0; c < 3; ++c)
    {
        CHECK(ApiLoadCircuit(c) == ApiStatus::Ok);
        GraphView g = ApiGetGraph();
        RouteCompareView cmp = ApiCompareRoutes(Preset::Balanced);
        CHECK(cmp.candidates.count >= 2);
        for (int i = 0; i < cmp.candidates.count; ++i)
        {
            const RouteCandidateView& cand = cmp.candidates[i];
            CHECK(EdgesMatchVertices(g, cand.vertexPath, cand.edgePath));
            if (IsAlternativeLabel(cand.label))
            {
                CHECK_EQ(cand.sourcePreset, -1);
            }
            else
            {
                CHECK(cand.sourcePreset >= 0 && cand.sourcePreset <= 3);
            }
        }
    }
    ApiShutdown();
}

TEST(ApiV11_CompareRaceRoutes_OrderedLapsDrivenByTheirPreset)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    CHECK(ApiLoadCircuit(0) == ApiStatus::Ok);   // Harbor: Safety picks a different lap
    GraphView g = ApiGetGraph();
    int start = VertexWithType(g, VertexType::Start);
    int finish = VertexWithType(g, VertexType::Finish);

    RouteCompareView cmp = ApiCompareRaceRoutes(Preset::Balanced);
    CHECK(cmp.candidates.count >= 2);
    int count = cmp.candidates.count;

    for (int i = 0; i < count; ++i)
    {
        cmp = ApiCompareRaceRoutes(Preset::Balanced);   // re-read: ApiPlanLap below is a different call
        ArrayView<double> costs = ApiEdgeCosts(Preset::Balanced);
        const RouteCandidateView& cand = cmp.candidates[i];
        CHECK(cand.found);
        CHECK(EdgesMatchVertices(g, cand.vertexPath, cand.edgePath));
        CHECK_EQ(cand.vertexPath[0], start);
        CHECK_EQ(cand.vertexPath[cand.vertexPath.count - 1], finish);

        // Checkpoints 1..10 are reached in order.
        int next = 1;
        for (int k = 0; k < cand.vertexPath.count; ++k)
        {
            if (g.vertices[cand.vertexPath[k]].checkpointOrder == next) ++next;
        }
        CHECK_EQ(next, 11);

        CHECK(Close(SumCosts(costs, cand.edgePath), cand.totalCost));

        // The candidate IS the lap the autopilot plans under its preset.
        CHECK(cand.sourcePreset >= 0 && cand.sourcePreset <= 3);
        RouteView lap = ApiPlanLap(static_cast<Preset>(cand.sourcePreset), RouteAlgo::AStar);
        CHECK_EQ(lap.edgePath.count, cand.edgePath.count);
        for (int k = 0; k < lap.edgePath.count && k < cand.edgePath.count; ++k) CHECK_EQ(lap.edgePath[k], cand.edgePath[k]);
    }
    ApiShutdown();
}

TEST(ApiV11_EdgeCosts_FollowLiveRaceEffects)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    CHECK(ApiLoadCircuit(0) == ApiStatus::Ok);
    // Harbor's script: oil spill on road 3 at 15 s, road 0 blocked at 40 s.
    double oilBefore = ApiEdgeCosts(Preset::Safety)[3];
    CHECK(ApiEdgeCosts(Preset::Safety)[0] >= 0.0);

    CHECK(ApiRaceStart() == ApiStatus::Ok);
    StepUntil(16000);
    CHECK(ApiEdgeCosts(Preset::Safety)[3] > oilBefore);
    StepUntil(41000);
    CHECK(ApiEdgeCosts(Preset::Safety)[0] < 0.0);   // blocked
    ApiShutdown();
}

TEST(ApiV11_LabRunInfo_ReportsTheRealDataSource)
{
    CHECK(ApiInit("data") == ApiStatus::Ok);
    CHECK(ApiLoadCircuit(0) == ApiStatus::Ok);
    GraphView g = ApiGetGraph();
    int start = VertexWithType(g, VertexType::Start);
    int cp5 = CheckpointVertexV(g, 5);
    LabInput input = ApiLabDefaultInput();
    CHECK(!input.useRaceState);

    CHECK(ApiLabRun(LabAlgo::Bfs, input) == ApiStatus::Ok);
    LabRunInfo info = ApiLabRunInfo();
    CHECK(info.source == LabDataSource::LoadedCircuit);
    CHECK_EQ(info.sourceVertex, start);
    CHECK_EQ(info.targetVertex, cp5);
    CHECK(HasText(info.description));

    input.useBriefExample = true;
    CHECK(ApiLabRun(LabAlgo::KnapsackDp, input) == ApiStatus::Ok);
    CHECK(ApiLabRunInfo().source == LabDataSource::BriefExample);
    input.useBriefExample = false;
    CHECK(ApiLabRun(LabAlgo::KnapsackDp, input) == ApiStatus::Ok);
    CHECK(ApiLabRunInfo().source == LabDataSource::LoadedCircuit);

    CHECK(ApiLabRun(LabAlgo::MergeSort, input) == ApiStatus::Ok);
    CHECK(ApiLabRunInfo().source == LabDataSource::RandomKeys);
    CHECK(ApiLabRun(LabAlgo::EventQueue, input) == ApiStatus::Ok);
    CHECK(ApiLabRunInfo().source == LabDataSource::CircuitEvents);
    CHECK(ApiLabRun(LabAlgo::TrackGenerator, input) == ApiStatus::Ok);
    CHECK(ApiLabRunInfo().source == LabDataSource::GeneratedTrack);

    // H7: live race state.
    CHECK(ApiLoadCircuit(0) == ApiStatus::Ok);
    CHECK(ApiRaceStart() == ApiStatus::Ok);
    StepUntil(30000);

    CHECK(ApiLabRun(LabAlgo::Dijkstra, input) == ApiStatus::Ok);
    CHECK(ApiLabRunInfo().source == LabDataSource::LiveRace);   // live track effects

    CHECK(ApiLabRun(LabAlgo::RefuelDp, input) == ApiStatus::Ok);
    info = ApiLabRunInfo();
    CHECK(info.source == LabDataSource::LoadedCircuit);          // baseline unless asked
    CHECK_EQ(info.sourceVertex, start);

    input.useRaceState = true;
    CHECK(ApiLabRun(LabAlgo::RefuelDp, input) == ApiStatus::Ok);
    info = ApiLabRunInfo();
    CHECK(info.source == LabDataSource::LiveRace);
    const RacerView* h = Human(ApiRaceSnapshot());
    CHECK(h != nullptr);
    if (h != nullptr && h->currentVertex >= 0) CHECK_EQ(info.sourceVertex, h->currentVertex);
    CHECK(ApiLabStepCount() > 0);

    CHECK(ApiLabRun(LabAlgo::GreedyPickup, input) == ApiStatus::Ok);
    LabDataSource s = ApiLabRunInfo().source;
    CHECK(s == LabDataSource::LiveRace || s == LabDataSource::BriefExampleFallback);
    ApiShutdown();
}
