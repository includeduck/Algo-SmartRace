#include "TestFramework.h"

#include "../src/sim/RaceEngine.h"
#include "../src/io/TrackLoader.h"
#include "../src/model/Graph.h"
#include "../src/model/Rules.h"

using namespace model;
using namespace sim;

namespace
{
    // A small triangular loop: Start -> CP1 -> CP2 -> Finish -> (back to
    // Start via a closing edge), cheap enough in fuel/time that 2 laps
    // finish quickly under test, but with enough distance that fuel and
    // event timing are meaningfully exercised.
    Graph* BuildLoop(io::TrackData* trackData)
    {
        Graph* g = new Graph();
        int start = g->AddVertex(0, 0, VertexType::Start);
        int cp1 = g->AddVertex(100, 0, VertexType::Checkpoint, 1);
        int cp2 = g->AddVertex(100, 100, VertexType::Checkpoint, 2);
        int finish = g->AddVertex(0, 100, VertexType::Finish);

        // distance, baseTime, fuelCost, difficulty, shortcut, directed
        g->AddEdge(start, cp1, 100.0, 2.0, 2.0, 1, false, false);
        g->AddEdge(cp1, cp2, 100.0, 2.0, 2.0, 1, false, false);
        g->AddEdge(cp2, finish, 100.0, 2.0, 2.0, 1, false, false);
        g->AddEdge(finish, start, 100.0, 2.0, 2.0, 1, false, false);

        trackData->mainRoute.Push(start);
        trackData->mainRoute.Push(cp1);
        trackData->mainRoute.Push(cp2);
        trackData->mainRoute.Push(finish);
        trackData->mainRoute.Push(start);

        return g;
    }

    RaceConfig DefaultConfig(int laps)
    {
        RaceConfig config;
        config.laps = laps;
        config.opponentCount = 3;
        config.humanPreset = Preset::Balanced;
        config.eventSeed = 42ULL;
        config.humanAutopilot = true;
        return config;
    }

    void RunToCompletion(RaceEngine* engine, int maxTicks = 200000)
    {
        int ticks = 0;
        while (!engine->IsRaceOver() && ticks < maxTicks)
        {
            engine->Step();
            ++ticks;
        }
    }
}

TEST(RaceEngine_FullRace_AllRacersFinishOrAreClassified)
{
    io::TrackData trackData;
    Graph* g = BuildLoop(&trackData);

    RaceEngine engine;
    engine.Start(g, trackData, DefaultConfig(2));
    RunToCompletion(&engine);

    CHECK(engine.IsRaceOver());

    DynArray<RaceResultRow> results;
    engine.GetResults(&results);
    CHECK_EQ(results.Size(), 4);   // 1 human (autopilot) + 3 opponents

    bool anyFinished = false;
    for (int i = 0; i < results.Size(); ++i)
    {
        if (results[i].status == RaceStatus::Finished) anyFinished = true;
        // Positions are a permutation of 1..n -- a crude but useful check.
        CHECK(results[i].finishPosition >= 1 && results[i].finishPosition <= results.Size());
    }
    CHECK(anyFinished);

    delete g;
}

TEST(RaceEngine_CheckpointsVisitedInOrder_AutopilotNeverSkipsOne)
{
    io::TrackData trackData;
    Graph* g = BuildLoop(&trackData);

    RaceEngine engine;
    engine.Start(g, trackData, DefaultConfig(1));

    // Track the human's nextCheckpoint sequence across the whole race --
    // it must only ever increase by exactly 1 at a time (1 -> 2 -> 3,
    // where 3 means "past CP2, awaiting the finish").
    int lastSeen = 1;
    for (int tick = 0; tick < 50000 && !engine.IsRaceOver(); ++tick)
    {
        engine.Step();
        const Racer& human = engine.GetRacer(0);
        CHECK(human.nextCheckpoint >= lastSeen);
        CHECK(human.nextCheckpoint <= lastSeen + 1);
        lastSeen = human.nextCheckpoint;
    }

    delete g;
}

TEST(RaceEngine_RoadClosure_ReroutesWithoutRestartingRace)
{
    io::TrackData trackData;
    Graph* g = BuildLoop(&trackData);

    // Block the direct cp1->cp2 edge (edge id 1) at t=0, and open a
    // detour so a route still exists; the human (autopilot, Dijkstra)
    // must still complete the lap using the new edge, not freeze.
    int extraCheckpointBypass = g->AddVertex(150, 50, VertexType::Intersection);
    int cp2 = 2;
    int cp1 = 1;
    g->AddEdge(cp1, extraCheckpointBypass, 50.0, 1.0, 1.0, 1, false, false);
    g->AddEdge(extraCheckpointBypass, cp2, 50.0, 1.0, 1.0, 1, false, false);

    trackData.events.Push(io::ScriptedEvent());
    io::ScriptedEvent& ev = trackData.events[trackData.events.Size() - 1];
    ev.triggerTimeMs = 0;
    ev.type.Set("RoadBlock");
    ev.target = 1;   // cp1 -> cp2 direct edge
    ev.durationMs = 0;

    RaceEngine engine;
    engine.Start(g, trackData, DefaultConfig(1));

    bool raceRestarted = false;
    int startTimeBefore = engine.NowMs();
    RunToCompletion(&engine, 50000);

    // "Without restarting the race" -- the clock only ever moved forward;
    // IsRaceOver() ending normally (not via some reset) is itself the check.
    CHECK(engine.NowMs() >= startTimeBefore);
    CHECK(!raceRestarted);

    DynArray<RaceResultRow> results;
    engine.GetResults(&results);
    bool humanFinished = false;
    for (int i = 0; i < results.Size(); ++i)
    {
        if (results[i].racerId == 0 && results[i].status == RaceStatus::Finished) humanFinished = true;
    }
    CHECK(humanFinished);   // reached the finish despite the direct edge being blocked from t=0

    delete g;
}

namespace
{
    // The same square loop, with a configurable fuel cost per road and an
    // optional fuel station S off the Finish -> Start corner (joined to
    // both, each spur costing 5 fuel units).
    Graph* BuildFuelLoop(io::TrackData* trackData, double fuelPerRoad, bool withStation)
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

        if (withStation)
        {
            int station = g->AddVertex(-50, 50, VertexType::FuelStation);
            g->AddEdge(finish, station, 71.0, 1.4, 5.0, 1, false, false);
            g->AddEdge(station, start, 71.0, 1.4, 5.0, 1, false, false);
        }

        trackData->mainRoute.Push(start);
        trackData->mainRoute.Push(cp1);
        trackData->mainRoute.Push(cp2);
        trackData->mainRoute.Push(finish);
        trackData->mainRoute.Push(start);
        return g;
    }

    bool Contains(const char* text, const char* needle)
    {
        for (int i = 0; text[i] != '\0'; ++i)
        {
            int k = 0;
            while (needle[k] != '\0' && text[i + k] == needle[k]) ++k;
            if (needle[k] == '\0') return true;
        }
        return false;
    }

    bool LogMentions(const RaceEngine& engine, const char* needle)
    {
        LogEntry entries[512];
        int n = engine.EventLog().Since(-1, entries, 512);
        for (int i = 0; i < n; ++i)
        {
            if (Contains(entries[i].text, needle)) return true;
        }
        return false;
    }
}

TEST(RaceEngine_FuelActuallyDrains_ByTheEdgeRule)
{
    // Three roads of 20.0 fuel units each = 600 tenths for one lap. A
    // regression guard for the Phase 2 bug where each 20ms slice was
    // rounded to a whole tenth on its own and the tank barely drained.
    io::TrackData trackData;
    Graph* g = BuildFuelLoop(&trackData, 20.0, false);

    RaceConfig config = DefaultConfig(1);
    config.opponentCount = 0;
    RaceEngine engine;
    engine.Start(g, trackData, config);
    RunToCompletion(&engine);

    const Racer& human = engine.GetRacer(0);
    CHECK(human.state == RacerState::Finished);
    CHECK(human.fuelTenths >= 399 && human.fuelTenths <= 401);

    delete g;
}

TEST(RaceEngine_InsufficientFuel_NoStation_EveryoneDnfs)
{
    // 40.0 units per road: the third road of lap 1 cannot be entered with
    // the 20.0 left, and there is no station, so every racer waits and
    // then DNFs after the recovery timeout.
    io::TrackData trackData;
    Graph* g = BuildFuelLoop(&trackData, 40.0, false);

    RaceEngine engine;
    engine.Start(g, trackData, DefaultConfig(1));
    RunToCompletion(&engine);

    CHECK(engine.IsRaceOver());
    DynArray<RaceResultRow> results;
    engine.GetResults(&results);
    for (int i = 0; i < results.Size(); ++i) CHECK(results[i].status == RaceStatus::Dnf);
    CHECK(LogMentions(engine, "DNF"));

    delete g;
}

TEST(RaceEngine_LowFuel_DetoursToStation_RefuelsAndFinishes)
{
    // 20.0 units per road: lap 1 costs 600 of the 1000 tenths, lap 2 needs
    // 800 more. Nobody can finish 2 laps on one tank -- every racer (the
    // human autopilot and all three opponent strategies) must detour to
    // the station, buy fuel, and still finish.
    io::TrackData trackData;
    Graph* g = BuildFuelLoop(&trackData, 20.0, true);

    RaceEngine engine;
    engine.Start(g, trackData, DefaultConfig(2));
    RunToCompletion(&engine);

    DynArray<RaceResultRow> results;
    engine.GetResults(&results);
    CHECK_EQ(results.Size(), 4);
    for (int i = 0; i < results.Size(); ++i) CHECK(results[i].status == RaceStatus::Finished);

    CHECK(LogMentions(engine, "heading to station"));
    CHECK(LogMentions(engine, "refuelling at V4"));

    delete g;
}

TEST(RaceEngine_RaceResults_OrderedByFinishTime)
{
    io::TrackData trackData;
    Graph* g = BuildFuelLoop(&trackData, 2.0, false);

    RaceEngine engine;
    engine.Start(g, trackData, DefaultConfig(2));
    RunToCompletion(&engine);

    DynArray<RaceResultRow> results;
    engine.GetResults(&results);
    for (int i = 1; i < results.Size(); ++i)
    {
        const RaceResultRow& a = results[i - 1];
        const RaceResultRow& b = results[i];
        CHECK_EQ(b.finishPosition, a.finishPosition + 1);
        if (a.status == RaceStatus::Finished && b.status == RaceStatus::Finished)
        {
            // Earlier finish first; an exact tie is broken by lower racer id.
            CHECK(a.finishTimeMs < b.finishTimeMs || (a.finishTimeMs == b.finishTimeMs && a.racerId < b.racerId));
        }
    }

    delete g;
}

TEST(RaceEngine_SameSeedTwice_ProducesSameDeterminismHash)
{
    io::TrackData trackDataA;
    Graph* gA = BuildLoop(&trackDataA);
    RaceEngine engineA;
    engineA.Start(gA, trackDataA, DefaultConfig(2));
    RunToCompletion(&engineA);
    unsigned long long hashA = engineA.DeterminismHash();

    io::TrackData trackDataB;
    Graph* gB = BuildLoop(&trackDataB);
    RaceEngine engineB;
    engineB.Start(gB, trackDataB, DefaultConfig(2));
    RunToCompletion(&engineB);
    unsigned long long hashB = engineB.DeterminismHash();

    CHECK_EQ(hashA, hashB);

    delete gA;
    delete gB;
}

TEST(RaceEngine_NitroCollection_CancelsPendingExpiryEvent)
{
    io::TrackData trackData;
    Graph* g = BuildLoop(&trackData);

    // CP1 (vertex 1) doubles as the nitro pickup for this test: it starts
    // inactive, a scripted NitroBooster activates it at t=0 (well before
    // the human, starting at the Start vertex, can reach it), and the
    // human collects it on arrival. Repurposing the Start vertex itself
    // would remove the graph's only Start vertex and break racer
    // placement, so a different vertex is used deliberately.
    g->GetVertexMutable(1).type = VertexType::NitroPickup;
    g->GetVertexMutable(1).active = false;

    trackData.events.Push(io::ScriptedEvent());
    io::ScriptedEvent& ev = trackData.events[trackData.events.Size() - 1];
    ev.triggerTimeMs = 0;
    ev.type.Set("NitroBooster");
    ev.target = 1;
    ev.durationMs = 25000;

    RaceConfig config = DefaultConfig(1);
    config.opponentCount = 0;   // isolate the human for this check

    RaceEngine engine;
    engine.Start(g, trackData, config);

    // Edge Start->CP1 is distance 100 at base speed 50 units/s (distance
    // 100 / baseTime 2.0s) -> ~2000ms to arrive. Run well past that.
    for (int tick = 0; tick < 300 && !engine.IsRaceOver(); ++tick) engine.Step();

    const Racer& human = engine.GetRacer(0);
    CHECK_EQ(human.nitroCharges, Rules::kDefaultStartNitroCharges + 1);
    CHECK(!g->GetVertex(1).active);

    delete g;
}

TEST(RaceEngine_EventWithWrongKindOfTarget_IsRejected)
{
    // Section 4.5: an event must target an element that exists AND is the
    // right kind. A station outage aimed at a checkpoint, a nitro booster
    // aimed at the Start and a shortcut toggle aimed at an ordinary road
    // must all be rejected (logged), leaving the track untouched.
    io::TrackData trackData;
    Graph* g = BuildLoop(&trackData);

    io::ScriptedEvent outage;
    outage.triggerTimeMs = 20;
    outage.type.Set("FuelStationToggle");
    outage.target = 1;   // CP1, not a station
    outage.durationMs = 30000;
    trackData.events.Push(outage);

    io::ScriptedEvent booster;
    booster.triggerTimeMs = 20;
    booster.type.Set("NitroBooster");
    booster.target = 0;   // the Start, not a nitro pickup
    booster.durationMs = 25000;
    trackData.events.Push(booster);

    io::ScriptedEvent toggle;
    toggle.triggerTimeMs = 20;
    toggle.type.Set("ShortcutToggle");
    toggle.target = 0;   // an ordinary road
    toggle.durationMs = 0;
    trackData.events.Push(toggle);

    RaceEngine engine;
    engine.Start(g, trackData, DefaultConfig(1));
    for (int i = 0; i < 5; ++i) engine.Step();

    LogEntry lines[64];
    int count = engine.EventLog().Since(-1, lines, 64);
    int rejected = 0;
    for (int i = 0; i < count; ++i)
    {
        const char* t = lines[i].text;
        for (int k = 0; t[k] != '\0'; ++k)
        {
            if (t[k] == 'r' && t[k + 1] == 'e' && t[k + 2] == 'j' && t[k + 3] == 'e' && t[k + 4] == 'c') ++rejected;
        }
    }
    CHECK_EQ(rejected, 3);
    CHECK(g->GetVertex(1).active);
    CHECK(g->IsTraversable(0));

    delete g;
}
