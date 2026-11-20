#include "TestFramework.h"

#include "../src/sim/RaceEngine.h"
#include "../src/sim/ResourcePlanner.h"
#include "../src/io/TrackLoader.h"
#include "../src/model/Graph.h"
#include "../src/model/Rules.h"

#include <cstdio>

// Regression tests for the completeness findings Rayyan raised on
// 19 Nov 2026 (docs/gui-backend-handoff.md, F01-F05). Each test fails on the
// code as it was before the fix.

using namespace model;
using namespace sim;

namespace
{
    RaceConfig OneRacerConfig(int laps)
    {
        RaceConfig config;
        config.laps = laps;
        config.opponentCount = 0;
        config.humanPreset = Preset::Balanced;
        config.eventSeed = 42ULL;
        config.humanAutopilot = true;
        return config;
    }

    void AddEvent(io::TrackData* data, int timeMs, const char* type, int target, int durationMs)
    {
        io::ScriptedEvent ev;
        ev.triggerTimeMs = timeMs;
        ev.type.Set(type);
        ev.target = target;
        ev.durationMs = durationMs;
        data->events.Push(ev);
    }

    void StepUntil(RaceEngine* engine, int timeMs)
    {
        while (engine->NowMs() < timeMs && !engine->IsRaceOver()) engine->Step();
    }

    bool Mentions(const RaceEngine& engine, const char* needle)
    {
        LogEntry entries[512];
        int n = engine.EventLog().Since(-1, entries, 512);
        for (int i = 0; i < n; ++i)
        {
            const char* t = entries[i].text;
            for (int k = 0; t[k] != '\0'; ++k)
            {
                int j = 0;
                while (needle[j] != '\0' && t[k + j] == needle[j]) ++j;
                if (needle[j] == '\0') return true;
            }
        }
        return false;
    }

    // Start(0) -> CP1(1) -> Finish(2), closed by Finish -> Start, with a
    // second way from CP1 to Finish through Y(3). `directDistance` sets the
    // direct CP1 -> Finish road (edge 1); the Y roads are edges 2 and 3.
    Graph* BuildFork(io::TrackData* data, double directDistance, bool yBlocked)
    {
        Graph* g = new Graph();
        int start = g->AddVertex(0, 0, VertexType::Start);
        int cp1 = g->AddVertex(100, 0, VertexType::Checkpoint, 1);
        int finish = g->AddVertex(200, 0, VertexType::Finish);
        int y = g->AddVertex(150, 50, VertexType::Intersection);

        // distance, baseTime (s), fuelCost, difficulty, shortcut, directed
        g->AddEdge(start, cp1, 100.0, 2.0, 2.0, 1, false, false);                           // 0
        g->AddEdge(cp1, finish, directDistance, directDistance / 50.0, 2.0, 1, false, false); // 1
        g->AddEdge(cp1, y, 51.0, 1.02, 1.0, 1, false, false,
            yBlocked ? EdgeStatus::Blocked : EdgeStatus::Open);                              // 2
        g->AddEdge(y, finish, 51.0, 1.02, 1.0, 1, false, false);                            // 3
        g->AddEdge(finish, start, 200.0, 4.0, 4.0, 1, false, false);                        // 4

        data->mainRoute.Push(start);
        data->mainRoute.Push(cp1);
        data->mainRoute.Push(finish);
        data->mainRoute.Push(start);
        return g;
    }

    // Whether the human drives road `edge` before `untilMs`. (Roads, not
    // vertices: a racer arriving at a vertex usually leaves it in the same
    // tick, so currentVertex is rarely seen between steps.)
    bool HumanDrives(RaceEngine* engine, int edge, int untilMs)
    {
        bool seen = false;
        while (engine->NowMs() < untilMs && !engine->IsRaceOver())
        {
            engine->Step();
            if (engine->GetRacer(0).currentEdge == edge) seen = true;
        }
        return seen;
    }
}

// ---------------------------------------------------------------- F01

TEST(F01_OilSpillAhead_AutopilotReplansAtNextVertex)
{
    // The direct road (100) is just cheaper than the Y route (102) at the
    // start. An oil spill on it while the human is still on the first road
    // makes it slower and harder: the human must switch to Y at CP1. Before
    // the fix only plans through a BLOCKED road were rebuilt, so the human
    // drove into the oil.
    io::TrackData data;
    Graph* g = BuildFork(&data, 100.0, false);
    AddEvent(&data, 200, "OilSpill", 1, 60000);

    RaceEngine engine;
    engine.Start(g, data, OneRacerConfig(1));
    CHECK(HumanDrives(&engine, 3, 20000));
    CHECK(engine.GetRacer(0).state == RacerState::Finished);
    delete g;
}

TEST(F01_ReopenedCheaperRoad_AutopilotTakesIt)
{
    // The Y route starts blocked, so the human plans the long direct road
    // (160). Y reopens while the human is still on the first road; the
    // cheaper reopened route must be taken from CP1 onwards.
    io::TrackData data;
    Graph* g = BuildFork(&data, 160.0, true);
    AddEvent(&data, 200, "RoadClear", 2, 0);

    RaceEngine engine;
    engine.Start(g, data, OneRacerConfig(1));
    CHECK(HumanDrives(&engine, 3, 20000));
    CHECK(engine.GetRacer(0).state == RacerState::Finished);
    delete g;
}

TEST(F01_NoGraphChange_PlanIsKept_AndFixedOpponentKeepsItsRoute)
{
    // Control: with no event the human takes the cheaper direct road, and
    // Opponent 1 (fixed main route) never leaves the main route even after
    // an oil spill makes Y cheaper -- fixed strategies are not rerouted by
    // cost changes, only by closures.
    io::TrackData calm;
    Graph* g1 = BuildFork(&calm, 100.0, false);
    RaceEngine quiet;
    quiet.Start(g1, calm, OneRacerConfig(1));
    CHECK(!HumanDrives(&quiet, 3, 20000));
    delete g1;

    io::TrackData data;
    Graph* g2 = BuildFork(&data, 100.0, false);
    AddEvent(&data, 200, "OilSpill", 1, 60000);
    RaceConfig config = OneRacerConfig(1);
    config.opponentCount = 3;
    RaceEngine engine;
    engine.Start(g2, data, config);
    bool fixedLeftRoute = false;
    while (engine.NowMs() < 20000 && !engine.IsRaceOver())
    {
        engine.Step();
        if (engine.GetRacer(1).currentEdge == 2 || engine.GetRacer(1).currentEdge == 3) fixedLeftRoute = true;
    }
    CHECK(engine.GetRacer(1).strategyType == StrategyType::FixedRoute);
    CHECK(!fixedLeftRoute);
    delete g2;
}

// ---------------------------------------------------------------- F02

namespace
{
    // Three 33.3-unit roads: one lap needs 999 of the 1000-tenth tank, so
    // nitro (double fuel) cannot be paid for anywhere on the lap.
    Graph* BuildTightLoop(io::TrackData* data, double fuelPerRoad)
    {
        Graph* g = new Graph();
        int start = g->AddVertex(0, 0, VertexType::Start);
        int cp1 = g->AddVertex(100, 0, VertexType::Checkpoint, 1);
        int cp2 = g->AddVertex(100, 100, VertexType::Checkpoint, 2);
        int finish = g->AddVertex(0, 100, VertexType::Finish);
        g->AddEdge(start, cp1, 100.0, 2.0, fuelPerRoad, 1, false, false);
        g->AddEdge(cp1, cp2, 100.0, 2.0, fuelPerRoad, 1, false, false);
        g->AddEdge(cp2, finish, 100.0, 2.0, fuelPerRoad, 1, false, false);
        g->AddEdge(finish, start, 100.0, 2.0, fuelPerRoad, 1, false, false);
        data->mainRoute.Push(start);
        data->mainRoute.Push(cp1);
        data->mainRoute.Push(cp2);
        data->mainRoute.Push(finish);
        data->mainRoute.Push(start);
        return g;
    }
}

TEST(F02_Nitro_NeverSpendsFuelTheTankDoesNotHave)
{
    io::TrackData data;
    Graph* g = BuildTightLoop(&data, 33.3);
    RaceEngine engine;
    engine.Start(g, data, OneRacerConfig(1));
    CHECK(engine.CmdActivateNitro());

    // The invariant on every tick: what is left (minus fuel already used
    // but not yet deducted) covers the rest of the road at the normal rate.
    bool invariantHeld = true;
    int ticks = 0;
    while (!engine.IsRaceOver() && ticks < 20000)
    {
        engine.Step();
        ++ticks;
        const Racer& h = engine.GetRacer(0);
        if (h.state == RacerState::OnEdge)
        {
            double available = h.fuelTenths - h.fuelRemainder;
            double reserve = EdgeFuelTenths(*g, h.currentEdge) * (1.0 - h.edgeProgress);
            if (available < reserve - 1e-6) invariantHeld = false;
        }
        if (h.fuelTenths < 0) invariantHeld = false;
    }
    CHECK(invariantHeld);
    CHECK(Mentions(engine, "nitro held"));
    // The boost on the first road was paid for honestly, so the tank is
    // now short for the last road: the car stops at a vertex (and is
    // classified DNF) instead of driving on fuel it never had.
    CHECK(engine.GetRacer(0).state == RacerState::Dnf);
    CHECK(engine.GetRacer(0).currentEdge == -1);
    delete g;
}

TEST(F02_HeldNitro_KeepsItsRemainingTime)
{
    // With exactly enough fuel for the lap at the normal rate the boost is
    // never affordable, so the held nitro must not count down.
    io::TrackData data;
    Graph* g = BuildTightLoop(&data, 33.3);
    RaceEngine engine;
    engine.Start(g, data, OneRacerConfig(1));
    // Drive the first two roads at the normal rate (666 tenths), then
    // activate on the third: 334 left against a 333 reserve cannot pay for
    // even one boosted tick.
    while (engine.GetRacer(0).currentEdge != 2 && !engine.IsRaceOver()) engine.Step();
    CHECK(engine.CmdActivateNitro());
    for (int i = 0; i < 10; ++i) engine.Step();
    const Racer& h = engine.GetRacer(0);
    CHECK(h.nitroActive);
    CHECK(h.nitroHeldForFuel);
    CHECK_EQ(h.nitroTimeLeftMs, Rules::kNitroDurationMs);
    delete g;
}

TEST(F02_AffordableNitro_StillBoostsAndExpires)
{
    io::TrackData plainData;
    Graph* plainGraph = BuildTightLoop(&plainData, 2.0);
    RaceEngine plain;
    plain.Start(plainGraph, plainData, OneRacerConfig(1));
    while (!plain.IsRaceOver()) plain.Step();

    io::TrackData data;
    Graph* g = BuildTightLoop(&data, 2.0);
    RaceEngine boosted;
    boosted.Start(g, data, OneRacerConfig(1));
    CHECK(boosted.CmdActivateNitro());
    while (!boosted.IsRaceOver()) boosted.Step();

    CHECK(!Mentions(boosted, "nitro held"));
    CHECK(!boosted.GetRacer(0).nitroActive);
    CHECK(boosted.GetRacer(0).finishTimeMs < plain.GetRacer(0).finishTimeMs);
    CHECK(boosted.GetRacer(0).fuelTenths < plain.GetRacer(0).fuelTenths);
    delete plainGraph;
    delete g;
}

// ---------------------------------------------------------------- F03

namespace
{
    // A slow loop (10 s roads) with an extra fuel station (4) and an extra
    // nitro pickup (5) hanging off the Start, off every racing line.
    Graph* BuildServiceLoop(io::TrackData* data)
    {
        Graph* g = new Graph();
        int start = g->AddVertex(0, 0, VertexType::Start);
        int cp1 = g->AddVertex(100, 0, VertexType::Checkpoint, 1);
        int cp2 = g->AddVertex(100, 100, VertexType::Checkpoint, 2);
        int finish = g->AddVertex(0, 100, VertexType::Finish);
        int station = g->AddVertex(-50, 0, VertexType::FuelStation);
        int pickup = g->AddVertex(0, -50, VertexType::NitroPickup);
        g->GetVertexMutable(pickup).active = false;
        g->AddEdge(start, cp1, 100.0, 10.0, 1.0, 1, false, false);
        g->AddEdge(cp1, cp2, 100.0, 10.0, 1.0, 1, false, false);
        g->AddEdge(cp2, finish, 100.0, 10.0, 1.0, 1, false, false);
        g->AddEdge(finish, start, 100.0, 10.0, 1.0, 1, false, false);
        g->AddEdge(start, station, 500.0, 50.0, 1.0, 1, false, false);
        g->AddEdge(start, pickup, 500.0, 50.0, 1.0, 1, false, false);
        data->mainRoute.Push(start);
        data->mainRoute.Push(cp1);
        data->mainRoute.Push(cp2);
        data->mainRoute.Push(finish);
        data->mainRoute.Push(start);
        return g;
    }
}

TEST(F03_OverlappingStationOutages_StationStaysClosedUntilTheLastEnds)
{
    io::TrackData data;
    Graph* g = BuildServiceLoop(&data);
    AddEvent(&data, 0, "FuelStationToggle", 4, 30000);
    AddEvent(&data, 10000, "FuelStationToggle", 4, 30000);

    RaceEngine engine;
    engine.Start(g, data, OneRacerConfig(2));
    StepUntil(&engine, 20000);
    CHECK(!g->GetVertex(4).active);
    StepUntil(&engine, 30100);
    CHECK(!g->GetVertex(4).active);   // the first outage is over, the second is not
    StepUntil(&engine, 40100);
    CHECK(g->GetVertex(4).active);
    delete g;
}

TEST(F03_OverlappingBoosters_PickupStaysLitUntilTheLastExpires)
{
    io::TrackData data;
    Graph* g = BuildServiceLoop(&data);
    AddEvent(&data, 0, "NitroBooster", 5, 25000);
    AddEvent(&data, 10000, "NitroBooster", 5, 25000);

    RaceEngine engine;
    engine.Start(g, data, OneRacerConfig(2));
    StepUntil(&engine, 25100);
    CHECK(g->GetVertex(5).active);    // first booster expired, second still on
    StepUntil(&engine, 35100);
    CHECK(!g->GetVertex(5).active);
    delete g;
}

TEST(F03_CollectingOneBooster_LeavesTheOtherRunning_AndItsStaleExpiryIsCancelled)
{
    // Two boosters on CP1, ending at 25 s and 45 s. The human collects one
    // on arrival (~10 s): the soonest one is consumed and its expiry
    // cancelled, the pickup stays lit, and it goes out at 45 s, not 25 s.
    io::TrackData data;
    Graph* g = BuildServiceLoop(&data);
    g->GetVertexMutable(1).type = VertexType::NitroPickup;
    g->GetVertexMutable(1).active = false;   // no base charge: only the boosters light it
    g->GetVertexMutable(1).checkpointOrder = -1;
    g->GetVertexMutable(2).checkpointOrder = 1;
    AddEvent(&data, 0, "NitroBooster", 1, 25000);
    AddEvent(&data, 0, "NitroBooster", 1, 45000);

    RaceEngine engine;
    engine.Start(g, data, OneRacerConfig(2));
    StepUntil(&engine, 12000);
    CHECK_EQ(engine.GetRacer(0).nitroCharges, Rules::kDefaultStartNitroCharges + 1);
    CHECK(g->GetVertex(1).active);
    StepUntil(&engine, 30000);
    CHECK(g->GetVertex(1).active);
    StepUntil(&engine, 45100);
    CHECK(!g->GetVertex(1).active);
    delete g;
}

TEST(F03_SameTimeOutageAndReactivation_ProcessedInHeapOrder)
{
    // An outage ends at 30 s exactly when a new one starts: whatever the
    // heap order, the station must be closed after both have run.
    io::TrackData data;
    Graph* g = BuildServiceLoop(&data);
    AddEvent(&data, 0, "FuelStationToggle", 4, 30000);
    AddEvent(&data, 30000, "FuelStationToggle", 4, 10000);

    RaceEngine engine;
    engine.Start(g, data, OneRacerConfig(2));
    StepUntil(&engine, 30100);
    CHECK(!g->GetVertex(4).active);
    StepUntil(&engine, 40100);
    CHECK(g->GetVertex(4).active);
    delete g;
}

// ---------------------------------------------------------------- F04

namespace
{
    // Start(0) -> CP1(1) -> Finish(2), each road 10 fuel units (100
    // tenths). Nitro N1(3) sits between Start and CP1, N2(4) between CP1 and
    // Finish; each detour uses 30 + 25 units instead of the 10-unit road,
    // i.e. 450 tenths extra.
    Graph* BuildPickupFork()
    {
        Graph* g = new Graph();
        int start = g->AddVertex(0, 0, VertexType::Start);
        int cp1 = g->AddVertex(100, 0, VertexType::Checkpoint, 1);
        int finish = g->AddVertex(200, 0, VertexType::Finish);
        int n1 = g->AddVertex(50, 40, VertexType::NitroPickup);
        int n2 = g->AddVertex(150, 40, VertexType::NitroPickup);
        g->AddEdge(start, cp1, 100.0, 2.0, 10.0, 1, false, false);
        g->AddEdge(cp1, finish, 100.0, 2.0, 10.0, 1, false, false);
        g->AddEdge(start, n1, 64.0, 1.3, 30.0, 1, false, false);
        g->AddEdge(n1, cp1, 64.0, 1.3, 25.0, 1, false, false);
        g->AddEdge(cp1, n2, 64.0, 1.3, 30.0, 1, false, false);
        g->AddEdge(n2, finish, 64.0, 1.3, 25.0, 1, false, false);
        g->AddEdge(finish, start, 200.0, 4.0, 10.0, 1, false, false);
        return g;
    }

    void LapRoute(DynArray<int>* v, DynArray<int>* e)
    {
        v->Clear();
        e->Clear();
        v->Push(0);
        v->Push(1);
        v->Push(2);
        e->Push(0);
        e->Push(1);
    }

    bool Offers(const DynArray<algo::PickupItem>& items, int vertex)
    {
        for (int i = 0; i < items.Size(); ++i)
        {
            if (items[i].vertex == vertex) return true;
        }
        return false;
    }
}

TEST(F04_DetourBeyondTheTank_IsNotOffered)
{
    Graph* g = BuildPickupFork();
    DynArray<int> v;
    DynArray<int> e;
    LapRoute(&v, &e);
    DynArray<algo::PickupItem> items;
    DynArray<PickupInfo> info;

    // 1000 tenths: both detours fit on their own.
    BuildPickupItems(*g, v, e, 0, 1000, 0, false, &items, &info);
    CHECK(Offers(items, 3));
    CHECK(Offers(items, 4));
    CHECK_EQ(info[0].extraFuelTenths, 450);

    // 300 tenths: the plain lap (200) is fine, but the access leg to N1
    // alone needs 300 and the way back 250 -- not offered. Before the fix
    // the planner priced detours by time only and offered both.
    BuildPickupItems(*g, v, e, 0, 300, 0, false, &items, &info);
    CHECK(!Offers(items, 3));
    CHECK(!Offers(items, 4));
    delete g;
}

TEST(F04_InactivePickup_IsNotOffered)
{
    Graph* g = BuildPickupFork();
    g->GetVertexMutable(3).active = false;
    DynArray<int> v;
    DynArray<int> e;
    LapRoute(&v, &e);
    DynArray<algo::PickupItem> items;
    DynArray<PickupInfo> info;
    BuildPickupItems(*g, v, e, 0, 1000, 0, false, &items, &info);
    CHECK(!Offers(items, 3));
    CHECK(Offers(items, 4));
    delete g;
}

TEST(F04_TwoDetoursThatFitAlone_DoNotFitTogether)
{
    Graph* g = BuildPickupFork();
    DynArray<int> v;
    DynArray<int> e;
    LapRoute(&v, &e);
    DynArray<algo::PickupItem> items;
    DynArray<PickupInfo> info;
    BuildPickupItems(*g, v, e, 0, 1000, 0, false, &items, &info);
    CHECK_EQ(items.Size(), 2);

    DynArray<int> first;
    first.Push(3);
    DynArray<int> second;
    second.Push(4);
    DynArray<int> both;
    both.Push(3);
    both.Push(4);
    // 200 + 450 = 650 alone; 200 + 900 = 1100 together > 1000.
    CHECK(PickupSelectionFeasible(*g, v, e, 1000, 0, items, info, first));
    CHECK(PickupSelectionFeasible(*g, v, e, 1000, 0, items, info, second));
    CHECK(!PickupSelectionFeasible(*g, v, e, 1000, 0, items, info, both));
    delete g;
}

TEST(F04_DetourThatWouldSkipCheckpointOrder_IsRejected)
{
    // A pickup reachable only THROUGH the finish from the first road:
    // Start -> P(3) is missing, P hangs off the Finish. Inserting it on the
    // Start -> CP1 road would reach the Finish before CP1, so the detoured
    // route ticks off a different set of checkpoints -- not offered there.
    Graph* g = new Graph();
    int start = g->AddVertex(0, 0, VertexType::Start);
    int cp1 = g->AddVertex(100, 0, VertexType::Checkpoint, 1);
    int cp2 = g->AddVertex(200, 0, VertexType::Checkpoint, 2);
    int finish = g->AddVertex(300, 0, VertexType::Finish);
    int p = g->AddVertex(0, -50, VertexType::NitroPickup);
    g->AddEdge(start, cp1, 100.0, 2.0, 1.0, 1, false, false);
    g->AddEdge(cp1, cp2, 100.0, 2.0, 1.0, 1, false, false);
    g->AddEdge(cp2, finish, 100.0, 2.0, 1.0, 1, false, false);
    g->AddEdge(start, cp2, 50.0, 1.0, 1.0, 1, false, false);   // a fast road straight to CP2
    g->AddEdge(cp2, p, 30.0, 0.6, 1.0, 1, false, false);
    DynArray<int> v;
    DynArray<int> e;
    v.Push(start);
    v.Push(cp1);
    v.Push(cp2);
    v.Push(finish);
    e.Push(0);
    e.Push(1);
    e.Push(2);
    DynArray<algo::PickupItem> items;
    DynArray<PickupInfo> info;
    BuildPickupItems(*g, v, e, 0, 1000, 0, false, &items, &info);
    // Offered, but never inserted before CP1 is reached: Start -> CP2 -> P
    // -> CP1 would visit CP2 first and still count it in order later, so the
    // insertion chosen must keep the same checkpoint progress.
    CHECK(Offers(items, p));
    CHECK(info[0].insertAt >= 1);
    delete g;
}

// ---------------------------------------------------------------- F05

namespace
{
    const char* kHeader = "CIRCUIT 0 \"F05\"\nLAPS 2\nVERTEX 0 0 0 Start\nVERTEX 1 100 0 Finish\n";

    io::LoadResult LoadOneEdge(const char* edgeLine, char* message, int capacity)
    {
        const char* path = "build/test_f05_edge.trk";
        FILE* f = nullptr;
        fopen_s(&f, path, "w");
        if (f == nullptr) return io::LoadResult::FileNotFound;
        fprintf(f, "%s%s\n", kHeader, edgeLine);
        fclose(f);
        io::TrackData data;
        int line = 0;
        message[0] = '\0';
        return io::LoadTrackFile(path, &data, &line, message, capacity);
    }

    bool Has(const char* text, const char* needle)
    {
        for (int i = 0; text[i] != '\0'; ++i)
        {
            int k = 0;
            while (needle[k] != '\0' && text[i + k] == needle[k]) ++k;
            if (needle[k] == '\0') return true;
        }
        return false;
    }
}

TEST(F05_Loader_RejectsBadEdgeNumbers_WithTheFieldNamed)
{
    char msg[200];
    CHECK(LoadOneEdge("EDGE 0 0 1 -5 2 1 1 0 0", msg, sizeof(msg)) == io::LoadResult::ParseError);
    CHECK(Has(msg, "distance"));
    CHECK(LoadOneEdge("EDGE 0 0 1 100 abc 1 1 0 0", msg, sizeof(msg)) == io::LoadResult::ParseError);
    CHECK(Has(msg, "baseTime"));
    CHECK(LoadOneEdge("EDGE 0 0 1 100 2 -1 1 0 0", msg, sizeof(msg)) == io::LoadResult::ParseError);
    CHECK(Has(msg, "fuelCost"));
    CHECK(LoadOneEdge("EDGE 0 0 1 100 2 1 0 0 0", msg, sizeof(msg)) == io::LoadResult::ParseError);
    CHECK(Has(msg, "difficulty"));
    CHECK(LoadOneEdge("EDGE 0 0 1 100 2 1 6 0 0", msg, sizeof(msg)) == io::LoadResult::ParseError);
    CHECK(LoadOneEdge("EDGE 0 0 1 nan 2 1 1 0 0", msg, sizeof(msg)) == io::LoadResult::ParseError);
    CHECK(LoadOneEdge("EDGE 0 0 1 1e999 2 1 1 0 0", msg, sizeof(msg)) == io::LoadResult::ParseError);
    CHECK(LoadOneEdge("EDGE 0 0 1 99999999999 2 1 1 0 0", msg, sizeof(msg)) == io::LoadResult::ParseError);
    CHECK(LoadOneEdge("EDGE 0 0 1 100 2 1 1 2 0", msg, sizeof(msg)) == io::LoadResult::ParseError);
    CHECK(Has(msg, "flags"));
}

TEST(F05_Loader_KeepsZeroAndBoundaryValues)
{
    char msg[200];
    CHECK(LoadOneEdge("EDGE 0 0 1 0 0 0 1 0 0", msg, sizeof(msg)) == io::LoadResult::Ok);
    CHECK(LoadOneEdge("EDGE 0 0 1 100.5 2.25 1.5 5 1 1", msg, sizeof(msg)) == io::LoadResult::Ok);
}
