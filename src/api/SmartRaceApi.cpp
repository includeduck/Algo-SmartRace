// SmartRace backend API implementation.
//
// Session/Main Menu, static track data, the Track Explorer queries and
// racing are all real as of Phase 2 (TrackLoader + Graph + BFS/DFS/
// Dijkstra/A*/RoutePlanner + the deterministic RaceEngine). Resource
// planning, championship and the Algorithm Lab are still to come.
#include "SmartRaceApi.h"
#include "ApiInternal.h"
#include "../sim/TrackValidator.h"
#include "../model/Rules.h"
#include "../model/CostModel.h"
#include "../io/TrackLoader.h"
#include "../algo/Bfs.h"
#include "../algo/Dfs.h"
#include "../algo/RoutePlanner.h"
#include "../sim/RaceEngine.h"
#include "../sim/ChampionshipManager.h"
#include "../sim/ResourcePlanner.h"
#include "../algo/Knapsack.h"
#include "../algo/RefuelDp.h"
#include "../core/util/Timer.h"
#include "../core/util/String.h"
#include "../core/util/MathUtil.h"
#include "../core/util/Log.h"
#include "../core/ds/DynArray.h"

#include <cmath>
#include <cstdio>

namespace
{
    struct CircuitEntry
    {
        const char* relativePath;
        const char* fallbackName;
    };

    const int kCircuitCount = 3;
    const CircuitEntry kCircuits[kCircuitCount] =
    {
        { "circuits/c1_harbor.trk", "Harbor Loop" },
        { "circuits/c2_twinpeaks.trk", "Twin Peaks" },
        { "circuits/c3_downtown.trk", "Downtown" },
    };

    bool g_initialized = false;
    bool g_circuitLoaded = false;
    bool g_raceRunning = false;
    bool g_raceOver = false;

    char g_lastError[256] = "";
    String g_dataDir;

    Settings g_settings =
    {
        Rules::kDefaultLaps,
        3,                                     // opponentCount
        Preset::Balanced,
        Rules::kDefaultStartNitroCharges,
        42ULL,                                  // eventSeed
        true,                                    // showCostLabels
    };

    // The currently loaded circuit. g_graph is a borrowed alias into
    // g_trackData.graph (TrackData owns it); loading a new circuit or
    // calling LoadTrackFile again frees the previous one automatically.
    io::TrackData g_trackData;
    model::Graph* g_graph = nullptr;
    String g_circuitName;
    int g_laps = Rules::kDefaultLaps;
    int g_startVertex = -1;
    int g_finishVertex = -1;
    model::CostReferences g_costReferences;

    DynArray<VertexView> g_vertexViews;
    DynArray<EdgeView> g_edgeViews;

    // The validator's verdict on the loaded circuit, computed once at load
    // (it runs several route searches and a DP, too much for every frame).
    sim::ValidationReport g_validation;
    DynArray<ValidationCheckView> g_validationViews;

    // Latest Explorer/planner results, kept alive so the ArrayViews handed
    // back to the GUI stay valid until the next call of the same kind (or
    // the next circuit load), per SmartRaceApi.h's lifetime contract.
    algo::BfsResult g_lastBfs;
    algo::DfsComponentsResult g_lastComponents;
    algo::DfsCycleResult g_lastCycles;
    algo::DfsDeadEndsResult g_lastDeadEnds;
    algo::PlanResult g_lastPlan;
    algo::RouteCompareResult g_lastCompare;
    DynArray<RouteCandidateView> g_lastCompareViews;

    // The real race engine (Phase 2).
    sim::RaceEngine* g_engine = nullptr;
    DynArray<RacerView> g_racerViews;
    DynArray<model::RaceResultRow> g_lastRaceResults;
    DynArray<ResultRowView> g_resultRowViews;
    DynArray<LogEntry> g_logEntryBuffer;
    DynArray<LogLineView> g_logViewBuffer;

    // Resource planning (Phase 3).
    DynArray<PickupItemView> g_pickupViews;
    DynArray<RefuelStopView> g_refuelStops;

    // Championship (Phase 3). Racer ids are stable across races (the
    // engine numbers racers the same way every race), so one name table
    // serves standings even between races, when no engine exists.
    sim::ChampionshipManager g_championship;
    bool g_championshipActive = false;
    int g_championshipCircuit = 0;
    const int kMaxRacerNames = 16;
    char g_racerNames[kMaxRacerNames][32];
    DynArray<StandingRowView> g_standingRows;
    DynArray<StandingRowView> g_topKRows;
    DynArray<ResultRowView> g_historyRows;

    void SetError(const char* message)
    {
        int i = 0;
        while (message[i] != '\0' && i < static_cast<int>(sizeof(g_lastError)) - 1)
        {
            g_lastError[i] = message[i];
            ++i;
        }
        g_lastError[i] = '\0';
    }

    VertexType ToApiVertexType(model::VertexType t)
    {
        switch (t)
        {
            case model::VertexType::Start: return VertexType::Start;
            case model::VertexType::Finish: return VertexType::Finish;
            case model::VertexType::Checkpoint: return VertexType::Checkpoint;
            case model::VertexType::FuelStation: return VertexType::FuelStation;
            case model::VertexType::NitroPickup: return VertexType::NitroPickup;
            case model::VertexType::DeadEnd: return VertexType::DeadEnd;
            case model::VertexType::Intersection:
            default: return VertexType::Intersection;
        }
    }

    EdgeStatus ToApiEdgeStatus(model::EdgeStatus s)
    {
        switch (s)
        {
            case model::EdgeStatus::Blocked: return EdgeStatus::Blocked;
            case model::EdgeStatus::Restricted: return EdgeStatus::Restricted;
            case model::EdgeStatus::Open:
            default: return EdgeStatus::Open;
        }
    }

    model::Preset ToModelPreset(Preset p)
    {
        switch (p)
        {
            case Preset::Speed: return model::Preset::Speed;
            case Preset::Economy: return model::Preset::Economy;
            case Preset::Safety: return model::Preset::Safety;
            case Preset::Balanced:
            default: return model::Preset::Balanced;
        }
    }

    algo::RouteAlgo ToAlgoRouteAlgo(RouteAlgo a)
    {
        return (a == RouteAlgo::AStar) ? algo::RouteAlgo::AStar : algo::RouteAlgo::Dijkstra;
    }

    RacerState ToApiRacerState(model::RacerState s)
    {
        switch (s)
        {
            case model::RacerState::OnEdge: return RacerState::OnEdge;
            case model::RacerState::Refuelling: return RacerState::Refuelling;
            case model::RacerState::Waiting: return RacerState::Waiting;
            case model::RacerState::Finished: return RacerState::Finished;
            case model::RacerState::Dnf: return RacerState::Dnf;
            case model::RacerState::AtVertex:
            default: return RacerState::AtVertex;
        }
    }

    void BuildViewsOf(const model::Graph& graph, DynArray<VertexView>* vertexViews, DynArray<EdgeView>* edgeViews)
    {
        vertexViews->Clear();
        for (int i = 0; i < graph.VertexCount(); ++i)
        {
            const model::Vertex& v = graph.GetVertex(i);
            VertexView vv;
            vv.id = v.id;
            vv.x = v.x;
            vv.y = v.y;
            vv.type = ToApiVertexType(v.type);
            vv.checkpointOrder = v.checkpointOrder;
            vv.active = v.active;
            vertexViews->Push(vv);
        }

        edgeViews->Clear();
        for (int i = 0; i < graph.EdgeCount(); ++i)
        {
            const model::Edge& e = graph.GetEdge(i);
            EdgeView ev;
            ev.id = e.id;
            ev.from = e.from;
            ev.to = e.to;
            ev.distance = e.distance;
            ev.baseTime = e.baseTime;
            ev.fuelCost = e.fuelCost;
            ev.difficulty = e.difficulty;
            ev.effDifficulty = e.effDifficulty;
            ev.status = ToApiEdgeStatus(e.status);
            ev.speedCap = e.speedCap;
            ev.isShortcut = e.isShortcut;
            ev.directed = e.directed;
            edgeViews->Push(ev);
        }
    }

    void RebuildViews()
    {
        BuildViewsOf(*g_graph, &g_vertexViews, &g_edgeViews);
    }

    void ComputeRacerPosition(const model::Racer& racer, double* outX, double* outY, double* outHeading)
    {
        if (racer.state == model::RacerState::OnEdge && racer.currentEdge >= 0)
        {
            const model::Edge& e = g_graph->GetEdge(racer.currentEdge);
            int fromVertex = (e.from == racer.currentTargetVertex) ? e.to : e.from;
            const model::Vertex& from = g_graph->GetVertex(fromVertex);
            const model::Vertex& to = g_graph->GetVertex(racer.currentTargetVertex);
            *outX = from.x + (to.x - from.x) * racer.edgeProgress;
            *outY = from.y + (to.y - from.y) * racer.edgeProgress;
            *outHeading = atan2(to.y - from.y, to.x - from.x);
            return;
        }

        if (racer.currentVertex >= 0)
        {
            const model::Vertex& v = g_graph->GetVertex(racer.currentVertex);
            *outX = v.x;
            *outY = v.y;
            *outHeading = 0.0;
            return;
        }

        *outX = 0.0;
        *outY = 0.0;
        *outHeading = 0.0;
    }

    // Matches RaceEngine::Start's naming: racer 0 is the human.
    void FillDefaultNames(int count)
    {
        for (int i = 0; i < count && i < kMaxRacerNames; ++i)
        {
            if (i == 0)
            {
                String::StrCopy(g_racerNames[i], 32, "You");
            }
            else
            {
                snprintf(g_racerNames[i], 32, "Racer %c", static_cast<char>('A' + ((i - 1) % 26)));
            }
        }
    }

    const char* RacerName(int racerId)
    {
        if (racerId < 0 || racerId >= kMaxRacerNames) return "?";
        return g_racerNames[racerId];
    }

    // Where the resource planners start from: the human's live state while
    // a race is running, otherwise the Start of a fresh race.
    struct PlannerOrigin
    {
        int vertex;
        int lastCompletedCheckpoint;
        int extraLaps;      // full laps after the current one
        int fuelTenths;
        int nitroCharges;
    };

    PlannerOrigin HumanOrigin()
    {
        PlannerOrigin origin;
        origin.vertex = g_startVertex;
        origin.lastCompletedCheckpoint = 0;
        origin.extraLaps = g_settings.laps - 1;
        origin.fuelTenths = Rules::kMaxFuelTenths;
        origin.nitroCharges = Rules::kDefaultStartNitroCharges;

        if (g_engine == nullptr || g_raceOver) return origin;

        for (int i = 0; i < g_engine->RacerCount(); ++i)
        {
            const model::Racer& racer = g_engine->GetRacer(i);
            if (!racer.isHuman) continue;

            // Mid-edge, plan from the vertex the car is about to reach.
            int at = (racer.currentVertex >= 0) ? racer.currentVertex : racer.currentTargetVertex;
            if (at >= 0) origin.vertex = at;
            origin.lastCompletedCheckpoint = racer.nextCheckpoint - 1;
            origin.extraLaps = g_settings.laps - racer.lap;
            origin.fuelTenths = racer.fuelTenths;
            origin.nitroCharges = racer.nitroCharges;
            break;
        }
        return origin;
    }

    bool ContainsId(const DynArray<int>& ids, int id)
    {
        for (int i = 0; i < ids.Size(); ++i)
        {
            if (ids[i] == id) return true;
        }
        return false;
    }

    sim::SortMethod ToSimSortMethod(SortMethod method)
    {
        switch (method)
        {
            case SortMethod::Quick: return sim::SortMethod::Quick;
            case SortMethod::Heap: return sim::SortMethod::Heap;
            case SortMethod::Merge:
            default: return sim::SortMethod::Merge;
        }
    }

    void FillStandingRows(const DynArray<model::ChampionshipEntry>& entries, DynArray<StandingRowView>* out)
    {
        out->Clear();
        for (int i = 0; i < entries.Size(); ++i)
        {
            const model::ChampionshipEntry& e = entries[i];
            StandingRowView row;
            row.position = i + 1;
            row.racerId = e.racerId;
            row.name = RacerName(e.racerId);
            row.totalPoints = e.totalPoints;
            row.wins = e.wins;
            row.sumOfFinishPositions = e.sumOfFinishPositions;
            row.bestFastestLapMs = e.bestFastestLapMs;
            out->Push(row);
        }
    }
}

ApiStatus ApiInit(const char* dataDir)
{
    g_initialized = true;
    g_circuitLoaded = false;
    g_raceRunning = false;
    g_raceOver = false;
    g_dataDir.Set(dataDir);
    g_championshipActive = false;
    delete g_engine;
    g_engine = nullptr;
    g_graph = nullptr;
    api_internal::ShutdownLab();
    FillDefaultNames(kMaxRacerNames);
    SetError("");
    return ApiStatus::Ok;
}

void ApiShutdown()
{
    g_initialized = false;
    g_circuitLoaded = false;
    g_raceRunning = false;
    g_raceOver = false;
    delete g_engine;
    g_engine = nullptr;
    g_graph = nullptr;   // borrowed alias into g_trackData, not owned here
    api_internal::ShutdownLab();
}

const char* ApiLastError()
{
    return g_lastError;
}

int ApiCircuitCount()
{
    if (!g_initialized) return 0;
    return kCircuitCount;
}

const char* ApiCircuitName(int index)
{
    if (index < 0 || index >= kCircuitCount) return "";
    return kCircuits[index].fallbackName;
}

namespace
{
    ApiStatus LoadFromPath(const char* path, const char* fallbackName)
    {
        int errorLine = 0;
        char errorMessage[200];
        io::LoadResult loadResult = io::LoadTrackFile(path, &g_trackData, &errorLine, errorMessage, sizeof(errorMessage));
        if (loadResult != io::LoadResult::Ok)
        {
            // A failed load leaves g_trackData cleared: nothing is loaded.
            g_circuitLoaded = false;
            g_graph = nullptr;
            delete g_engine;
            g_engine = nullptr;
            api_internal::ResetLab();

            char full[320];
            snprintf(full, sizeof(full), "%s (line %d): %s", path, errorLine, errorMessage);
            SetError(full);
            return ApiStatus::IoError;
        }

        g_graph = g_trackData.graph;   // borrowed; g_trackData owns it

        g_circuitName.Set(g_trackData.name.Empty() ? fallbackName : g_trackData.name.CStr());
        g_laps = g_trackData.laps;

        g_startVertex = -1;
        g_finishVertex = -1;
        for (int i = 0; i < g_graph->VertexCount(); ++i)
        {
            model::VertexType t = g_graph->GetVertex(i).type;
            if (t == model::VertexType::Start) g_startVertex = i;
            if (t == model::VertexType::Finish) g_finishVertex = i;
        }

        g_costReferences = model::ComputeReferences(*g_graph);
        RebuildViews();
        g_validation = sim::ValidateTrack(*g_graph, g_laps);

        g_circuitLoaded = true;
        g_raceRunning = false;
        g_raceOver = false;
        delete g_engine;
        g_engine = nullptr;
        api_internal::ResetLab();
        return ApiStatus::Ok;
    }
}

ApiStatus ApiLoadCircuit(int index)
{
    if (!g_initialized)
    {
        SetError("ApiInit was not called");
        return ApiStatus::NotInitialized;
    }
    if (index < 0 || index >= kCircuitCount)
    {
        SetError("no such circuit index");
        return ApiStatus::InvalidId;
    }

    String path(g_dataDir.CStr());
    path.Append("/");
    path.Append(kCircuits[index].relativePath);
    return LoadFromPath(path.CStr(), kCircuits[index].fallbackName);
}

ApiStatus ApiLoadCircuitFile(const char* path)
{
    if (!g_initialized)
    {
        SetError("ApiInit was not called");
        return ApiStatus::NotInitialized;
    }
    if (path == nullptr || path[0] == '\0')
    {
        SetError("no path given");
        return ApiStatus::InvalidArgument;
    }
    return LoadFromPath(path, "Custom circuit");
}

Settings ApiGetSettings()
{
    return g_settings;
}

ApiStatus ApiSetSettings(const Settings& settings)
{
    if (settings.laps < Rules::kMinLaps || settings.laps > Rules::kMaxLaps)
    {
        SetError("laps must be 2 or 3");
        return ApiStatus::InvalidArgument;
    }
    if (settings.opponentCount < 3 || settings.opponentCount > 9)
    {
        SetError("opponentCount must be 3..9");
        return ApiStatus::InvalidArgument;
    }
    if (settings.startNitroCharges < 0 || settings.startNitroCharges > Rules::kMaxNitroCharges)
    {
        SetError("startNitroCharges must be 0..3");
        return ApiStatus::InvalidArgument;
    }
    g_settings = settings;
    return ApiStatus::Ok;
}

GraphView ApiGetGraph()
{
    GraphView view;
    view.vertices.data = g_vertexViews.Data();
    view.vertices.count = g_circuitLoaded ? g_vertexViews.Size() : 0;
    view.edges.data = g_edgeViews.Data();
    view.edges.count = g_circuitLoaded ? g_edgeViews.Size() : 0;
    return view;
}

TrackInfo ApiGetTrackInfo()
{
    TrackInfo info;
    info.name = "";
    info.vertexCount = 0;
    info.edgeCount = 0;
    info.mandatoryCheckpointCount = 0;
    info.shortcutCount = 0;
    info.laps = g_settings.laps;
    info.hasCycle = false;
    info.hasDeadEnd = false;
    info.validatorPassed = false;

    if (!g_circuitLoaded) return info;

    info.name = g_circuitName.CStr();
    info.vertexCount = g_graph->VertexCount();
    info.edgeCount = g_graph->EdgeCount();
    info.laps = g_laps;

    int cpCount = 0;
    int shortcutCount = 0;
    for (int i = 0; i < g_graph->VertexCount(); ++i)
    {
        if (g_graph->GetVertex(i).checkpointOrder >= 0) ++cpCount;
    }
    for (int i = 0; i < g_graph->EdgeCount(); ++i)
    {
        if (g_graph->GetEdge(i).isShortcut) ++shortcutCount;
    }
    info.mandatoryCheckpointCount = cpCount;
    info.shortcutCount = shortcutCount;

    algo::DfsCycleResult cycles = algo::DfsCycles(*g_graph);
    info.hasCycle = cycles.hasCycle;

    algo::DfsDeadEndsResult deadEnds = algo::DfsDeadEnds(*g_graph, g_finishVertex);
    info.hasDeadEnd = deadEnds.deadEndVertices.Size() > 0;

    info.validatorPassed = g_validation.passed;

    return info;
}

ApiStatus ApiRaceStart()
{
    if (!g_circuitLoaded)
    {
        SetError("no circuit loaded");
        return ApiStatus::NotLoaded;
    }
    if (g_startVertex < 0 || g_finishVertex < 0)
    {
        SetError("circuit has no Start/Finish vertex");
        return ApiStatus::Infeasible;
    }

    delete g_engine;
    g_engine = new sim::RaceEngine();

    sim::RaceConfig config;
    config.laps = g_settings.laps;
    config.opponentCount = g_settings.opponentCount;
    config.humanPreset = ToModelPreset(g_settings.defaultPreset);
    config.eventSeed = g_settings.eventSeed;
    config.humanAutopilot = true;   // ApiCmdAutopilot(false) switches to manual
    config.startNitroCharges = g_settings.startNitroCharges;

    g_engine->Start(g_graph, g_trackData, config);
    RebuildViews();   // the engine's EffectManager may have already touched edge state

    for (int i = 0; i < g_engine->RacerCount() && i < kMaxRacerNames; ++i)
    {
        String::StrCopy(g_racerNames[i], 32, g_engine->GetRacer(i).name);
    }

    g_raceRunning = true;
    g_raceOver = false;
    return ApiStatus::Ok;
}

void ApiRaceStep(int ticks)
{
    if (g_engine == nullptr || !g_raceRunning || g_raceOver) return;

    for (int i = 0; i < ticks; ++i)
    {
        g_engine->Step();
        if (g_engine->IsRaceOver())
        {
            g_raceOver = true;
            break;
        }
    }

    RebuildViews();
}

RaceSnapshot ApiRaceSnapshot()
{
    RaceSnapshot snapshot;
    snapshot.timeMs = 0;
    snapshot.racers.data = nullptr;
    snapshot.racers.count = 0;
    snapshot.raceOver = g_raceOver;

    if (g_engine == nullptr) return snapshot;

    snapshot.timeMs = g_engine->NowMs();

    DynArray<model::RaceResultRow> liveResults;
    g_engine->GetResults(&liveResults);

    g_racerViews.Clear();
    for (int i = 0; i < g_engine->RacerCount(); ++i)
    {
        const model::Racer& racer = g_engine->GetRacer(i);
        RacerView rv;
        rv.id = racer.id;
        rv.name = racer.name;
        rv.isHuman = racer.isHuman;
        rv.state = ToApiRacerState(racer.state);
        ComputeRacerPosition(racer, &rv.x, &rv.y, &rv.heading);
        rv.currentVertex = racer.currentVertex;
        rv.currentEdge = racer.currentEdge;
        rv.edgeProgress = racer.edgeProgress;
        rv.fuelTenths = racer.fuelTenths;
        rv.nitroCharges = racer.nitroCharges;
        rv.nitroActive = racer.nitroActive;
        rv.lap = racer.lap;
        rv.nextCheckpoint = racer.nextCheckpoint;
        rv.totalTimeMs = racer.totalTimeMs;

        rv.livePosition = i + 1;
        for (int r = 0; r < liveResults.Size(); ++r)
        {
            if (liveResults[r].racerId == racer.id)
            {
                rv.livePosition = liveResults[r].finishPosition;
                break;
            }
        }

        g_racerViews.Push(rv);
    }

    snapshot.racers.data = g_racerViews.Data();
    snapshot.racers.count = g_racerViews.Size();
    return snapshot;
}

bool ApiRaceOver()
{
    return g_raceOver;
}

ApiStatus ApiCmdChooseEdge(int edgeId)
{
    if (g_engine == nullptr)
    {
        SetError("no race running");
        return ApiStatus::NotLoaded;
    }
    if (!g_engine->CmdChooseEdge(edgeId))
    {
        SetError(g_engine->LastCommandError());
        return ApiStatus::Illegal;
    }
    return ApiStatus::Ok;
}

ApiStatus ApiCmdNitro()
{
    if (g_engine == nullptr)
    {
        SetError("no race running");
        return ApiStatus::NotLoaded;
    }
    if (!g_engine->CmdActivateNitro())
    {
        SetError(g_engine->LastCommandError());
        return ApiStatus::Illegal;
    }
    return ApiStatus::Ok;
}

ApiStatus ApiCmdRefuel(int tenths)
{
    if (g_engine == nullptr)
    {
        SetError("no race running");
        return ApiStatus::NotLoaded;
    }
    if (!g_engine->CmdRefuel(tenths))
    {
        SetError(g_engine->LastCommandError());
        return ApiStatus::Illegal;
    }
    return ApiStatus::Ok;
}

ApiStatus ApiCmdAutopilot(bool on)
{
    if (g_engine == nullptr)
    {
        SetError("no race running");
        return ApiStatus::NotLoaded;
    }
    g_engine->CmdSetAutopilot(on);
    return ApiStatus::Ok;
}

ApiStatus ApiCmdPreset(Preset preset)
{
    if (g_engine == nullptr)
    {
        SetError("no race running");
        return ApiStatus::NotLoaded;
    }
    g_engine->CmdSetPreset(ToModelPreset(preset));
    return ApiStatus::Ok;
}

ResultsView ApiRaceResults()
{
    ResultsView view;
    view.rows.data = nullptr;
    view.rows.count = 0;

    if (g_engine == nullptr)
    {
        SetError("no race running");
        return view;
    }

    g_lastRaceResults.Clear();
    g_engine->GetResults(&g_lastRaceResults);

    g_resultRowViews.Clear();
    for (int i = 0; i < g_lastRaceResults.Size(); ++i)
    {
        const model::RaceResultRow& r = g_lastRaceResults[i];
        ResultRowView rv;
        rv.racerId = r.racerId;
        rv.finishPosition = r.finishPosition;
        rv.finishTimeMs = r.finishTimeMs;
        rv.bestLapTimeMs = r.bestLapTimeMs;
        rv.bonusPoints = r.bonusPoints;
        rv.pointsAwarded = r.pointsAwarded;
        rv.status = static_cast<int>(r.status);
        rv.lapsCompleted = r.lapsCompleted;
        g_resultRowViews.Push(rv);
    }

    view.rows.data = g_resultRowViews.Data();
    view.rows.count = g_resultRowViews.Size();
    return view;
}

LogView ApiRaceLogSince(int lastSeenIndex)
{
    LogView view;
    view.lines.data = nullptr;
    view.lines.count = 0;

    if (g_engine == nullptr) return view;

    const int kMaxPerCall = 64;
    LogEntry raw[kMaxPerCall];
    int n = g_engine->EventLog().Since(lastSeenIndex, raw, kMaxPerCall);

    // Two passes: first fill g_logEntryBuffer completely (so it stops
    // growing/reallocating), THEN point g_logViewBuffer's text pointers
    // into it -- otherwise an entry pointed at early in a single combined
    // loop could dangle once a later Push reallocates the buffer.
    g_logEntryBuffer.Clear();
    for (int i = 0; i < n; ++i) g_logEntryBuffer.Push(raw[i]);

    g_logViewBuffer.Clear();
    for (int i = 0; i < g_logEntryBuffer.Size(); ++i)
    {
        LogLineView lv;
        lv.index = g_logEntryBuffer[i].index;
        lv.timeMs = g_logEntryBuffer[i].timeMs;
        lv.text = g_logEntryBuffer[i].text;
        g_logViewBuffer.Push(lv);
    }

    view.lines.data = g_logViewBuffer.Data();
    view.lines.count = g_logViewBuffer.Size();
    return view;
}

// ---------------------------------------------------------------------- //
// Track Explorer / Route Planner
// ---------------------------------------------------------------------- //

ReachabilityView ApiRunBfs(int source, int target)
{
    ReachabilityView view;
    view.found = false;
    view.path.data = nullptr;
    view.path.count = 0;
    view.reachable.data = nullptr;
    view.reachable.count = 0;
    view.reachableCheckpoints.data = nullptr;
    view.reachableCheckpoints.count = 0;

    if (!g_circuitLoaded)
    {
        SetError("no circuit loaded");
        return view;
    }

    g_lastBfs = algo::Bfs(*g_graph, source, target);

    view.found = g_lastBfs.found;
    view.path.data = g_lastBfs.path.Data();
    view.path.count = g_lastBfs.path.Size();
    view.reachable.data = g_lastBfs.reachable.Data();
    view.reachable.count = g_lastBfs.reachable.Size();
    view.reachableCheckpoints.data = g_lastBfs.reachableCheckpoints.Data();
    view.reachableCheckpoints.count = g_lastBfs.reachableCheckpoints.Size();
    return view;
}

ComponentsView ApiRunDfsComponents()
{
    ComponentsView view;
    view.componentCount = 0;
    view.componentId.data = nullptr;
    view.componentId.count = 0;

    if (!g_circuitLoaded)
    {
        SetError("no circuit loaded");
        return view;
    }

    g_lastComponents = algo::DfsComponents(*g_graph);
    view.componentCount = g_lastComponents.componentCount;
    view.componentId.data = g_lastComponents.componentId.Data();
    view.componentId.count = g_lastComponents.componentId.Size();
    return view;
}

CycleView ApiRunDfsCycles()
{
    CycleView view;
    view.hasCycle = false;
    view.witnessCycle.data = nullptr;
    view.witnessCycle.count = 0;

    if (!g_circuitLoaded)
    {
        SetError("no circuit loaded");
        return view;
    }

    g_lastCycles = algo::DfsCycles(*g_graph);
    view.hasCycle = g_lastCycles.hasCycle;
    view.witnessCycle.data = g_lastCycles.witnessCycle.Data();
    view.witnessCycle.count = g_lastCycles.witnessCycle.Size();
    return view;
}

DeadEndsView ApiRunDeadEnds()
{
    DeadEndsView view;
    view.deadEndVertices.data = nullptr;
    view.deadEndVertices.count = 0;

    if (!g_circuitLoaded)
    {
        SetError("no circuit loaded");
        return view;
    }

    g_lastDeadEnds = algo::DfsDeadEnds(*g_graph, g_finishVertex);
    view.deadEndVertices.data = g_lastDeadEnds.deadEndVertices.Data();
    view.deadEndVertices.count = g_lastDeadEnds.deadEndVertices.Size();
    return view;
}

RouteView ApiPlanLap(Preset preset, RouteAlgo routeAlgo)
{
    RouteView view;
    view.found = false;
    view.blockedLegIndex = -1;
    view.vertexPath.data = nullptr;
    view.vertexPath.count = 0;
    view.edgePath.data = nullptr;
    view.edgePath.count = 0;
    view.totalCost = 0.0;
    view.totalDistance = 0.0;
    view.totalTime = 0.0;
    view.totalFuel = 0.0;

    if (!g_circuitLoaded)
    {
        SetError("no circuit loaded");
        return view;
    }
    if (g_startVertex < 0 || g_finishVertex < 0)
    {
        SetError("circuit has no Start/Finish vertex");
        return view;
    }

    model::Weights w = model::WeightsForPreset(ToModelPreset(preset));
    g_lastPlan = algo::PlanLap(*g_graph, g_startVertex, 0, g_finishVertex, w, g_costReferences,
        ToAlgoRouteAlgo(routeAlgo));

    view.found = g_lastPlan.found;
    view.blockedLegIndex = g_lastPlan.blockedLegIndex;
    view.vertexPath.data = g_lastPlan.vertexPath.Data();
    view.vertexPath.count = g_lastPlan.vertexPath.Size();
    view.edgePath.data = g_lastPlan.edgePath.Data();
    view.edgePath.count = g_lastPlan.edgePath.Size();
    view.totalCost = g_lastPlan.totalCost;
    view.totalDistance = g_lastPlan.totalDistance;
    view.totalTime = g_lastPlan.totalTime;
    view.totalFuel = g_lastPlan.totalFuel;
    return view;
}

RouteCompareView ApiCompareRoutes(Preset evalPreset)
{
    RouteCompareView view;
    view.candidates.data = nullptr;
    view.candidates.count = 0;

    if (!g_circuitLoaded)
    {
        SetError("no circuit loaded");
        return view;
    }
    if (g_startVertex < 0 || g_finishVertex < 0)
    {
        SetError("circuit has no Start/Finish vertex");
        return view;
    }

    model::Weights w = model::WeightsForPreset(ToModelPreset(evalPreset));
    g_lastCompare = algo::CompareRoutes(*g_graph, g_startVertex, g_finishVertex, w, g_costReferences);

    g_lastCompareViews.Clear();
    for (int i = 0; i < g_lastCompare.candidates.Size(); ++i)
    {
        const algo::RouteCandidate* c = g_lastCompare.candidates[i];
        RouteCandidateView cv;
        cv.label = c->label.CStr();
        cv.found = c->found;
        cv.vertexPath.data = c->vertexPath.Data();
        cv.vertexPath.count = c->vertexPath.Size();
        cv.totalCost = c->totalCost;
        cv.totalDistance = c->totalDistance;
        cv.totalTime = c->totalTime;
        cv.totalFuel = c->totalFuel;
        g_lastCompareViews.Push(cv);
    }

    view.candidates.data = g_lastCompareViews.Data();
    view.candidates.count = g_lastCompareViews.Size();
    return view;
}

// ---------------------------------------------------------------------- //
// Resource planning
// ---------------------------------------------------------------------- //

PickupView ApiPlanPickups(Preset preset, int budgetTenthsSec)
{
    PickupView view;
    view.items.data = nullptr;
    view.items.count = 0;
    view.budgetTenthsSec = budgetTenthsSec;
    view.dpBenefit = 0;
    view.dpCost = 0;
    view.greedyBenefit = 0;
    view.greedyCost = 0;
    view.dpStates = 0;
    view.fuelNeeded = false;

    if (!g_circuitLoaded)
    {
        SetError("no circuit loaded");
        return view;
    }
    if (budgetTenthsSec < 0)
    {
        SetError("budget must be >= 0");
        return view;
    }

    PlannerOrigin origin = HumanOrigin();
    model::Weights weights = model::WeightsForPreset(ToModelPreset(preset));

    // Fuel stations are only worth offering when the race cannot be
    // finished on the route as it stands -- the refuelling DP decides.
    DynArray<int> raceVertices;
    DynArray<int> raceEdges;
    if (sim::BuildRaceRoute(*g_graph, origin.vertex, origin.lastCompletedCheckpoint, origin.extraLaps,
        g_finishVertex, weights, g_costReferences, &raceVertices, &raceEdges))
    {
        algo::RefuelProblem problem;
        sim::BuildRefuelProblem(*g_graph, raceVertices, raceEdges, origin.fuelTenths, &problem);
        view.fuelNeeded = !algo::PlanRefuelling(problem).feasible;
    }

    // Detours are priced against the route for the rest of THIS lap.
    DynArray<int> lapVertices;
    DynArray<int> lapEdges;
    if (!sim::BuildRaceRoute(*g_graph, origin.vertex, origin.lastCompletedCheckpoint, 0,
        g_finishVertex, weights, g_costReferences, &lapVertices, &lapEdges))
    {
        SetError("no route from the current position");
        return view;
    }

    DynArray<algo::PickupItem> items;
    DynArray<sim::PickupInfo> info;
    sim::BuildPickupItems(*g_graph, lapVertices, lapEdges, origin.nitroCharges, view.fuelNeeded, &items, &info);

    algo::AlgoStats dpStats;
    algo::KnapsackResult dp = algo::KnapsackDp(items.Data(), items.Size(), budgetTenthsSec, &dpStats);
    algo::KnapsackResult greedy = algo::GreedyPickup(items.Data(), items.Size(), budgetTenthsSec);

    g_pickupViews.Clear();
    for (int i = 0; i < items.Size(); ++i)
    {
        PickupItemView pv;
        pv.vertex = items[i].vertex;
        pv.isNitro = info[i].isNitro;
        pv.costTenthsSec = items[i].cost;
        pv.benefitTenthsSec = items[i].benefit;
        pv.detourSeconds = info[i].detourSeconds;
        pv.chosenByDp = ContainsId(dp.selectedIds, items[i].id);
        pv.chosenByGreedy = ContainsId(greedy.selectedIds, items[i].id);
        g_pickupViews.Push(pv);
    }

    view.items.data = g_pickupViews.Data();
    view.items.count = g_pickupViews.Size();
    view.dpBenefit = dp.totalBenefit;
    view.dpCost = dp.totalCost;
    view.greedyBenefit = greedy.totalBenefit;
    view.greedyCost = greedy.totalCost;
    view.dpStates = dpStats.dpStates;
    return view;
}

RefuelView ApiPlanRefuel(Preset preset)
{
    RefuelView view;
    view.feasible = false;
    view.totalStopTimeMs = 0;
    view.finalFuelTenths = 0;
    view.stopCount = 0;
    view.stops.data = nullptr;
    view.stops.count = 0;

    if (!g_circuitLoaded)
    {
        SetError("no circuit loaded");
        return view;
    }

    PlannerOrigin origin = HumanOrigin();
    model::Weights weights = model::WeightsForPreset(ToModelPreset(preset));

    DynArray<int> vertices;
    DynArray<int> edges;
    if (!sim::BuildRaceRoute(*g_graph, origin.vertex, origin.lastCompletedCheckpoint, origin.extraLaps,
        g_finishVertex, weights, g_costReferences, &vertices, &edges))
    {
        SetError("no route from the current position");
        return view;
    }

    algo::RefuelProblem problem;
    sim::BuildRefuelProblem(*g_graph, vertices, edges, origin.fuelTenths, &problem);
    algo::RefuelPlan plan = algo::PlanRefuelling(problem);

    g_refuelStops.Clear();
    for (int j = 0; j < problem.legFuel.Size(); ++j)
    {
        RefuelStopView stop;
        stop.vertex = problem.stopVertex[j];
        stop.canBuy = problem.canBuyAt[j];
        stop.purchaseTenths = plan.feasible ? plan.purchase[j] : 0;
        g_refuelStops.Push(stop);
    }

    view.feasible = plan.feasible;
    view.totalStopTimeMs = plan.totalStopTimeMs;
    view.finalFuelTenths = plan.finalFuel;
    view.stopCount = plan.stopCount;
    view.stops.data = g_refuelStops.Data();
    view.stops.count = g_refuelStops.Size();
    if (!plan.feasible) SetError("no refuelling plan finishes the race on this route; a station detour is needed");
    return view;
}

int ApiRecommendedRefuel()
{
    if (g_engine == nullptr) return 0;
    return g_engine->RecommendedRefuel();
}

// ---------------------------------------------------------------------- //
// Championship
// ---------------------------------------------------------------------- //

ApiStatus ApiChampionshipStart()
{
    if (!g_initialized)
    {
        SetError("ApiInit was not called");
        return ApiStatus::NotInitialized;
    }

    int racers = 1 + g_settings.opponentCount;
    g_championship.Reset(racers);
    FillDefaultNames(racers);
    g_championshipActive = true;
    g_championshipCircuit = 0;
    return ApiLoadCircuit(0);
}

ApiStatus ApiChampionshipNextRace()
{
    if (!g_championshipActive)
    {
        SetError("no championship running");
        return ApiStatus::NotLoaded;
    }
    if (g_championship.IsComplete())
    {
        SetError("the championship is already complete");
        return ApiStatus::Illegal;
    }
    if (g_engine == nullptr || !g_raceOver)
    {
        SetError("the current race is not over yet");
        return ApiStatus::Illegal;
    }

    DynArray<model::RaceResultRow> results;
    g_engine->GetResults(&results);
    g_championship.ApplyRace(results);

    if (g_championship.IsComplete()) return ApiStatus::Ok;   // keep the last race viewable

    g_championshipCircuit = g_championship.RacesCompleted() % kCircuitCount;
    return ApiLoadCircuit(g_championshipCircuit);
}

ChampionshipStatus ApiChampionshipGetStatus()
{
    ChampionshipStatus status;
    status.active = g_championshipActive;
    status.racesCompleted = g_championshipActive ? g_championship.RacesCompleted() : 0;
    status.racesTotal = Rules::kChampionshipRaces;
    status.currentCircuitIndex = g_championshipCircuit;
    status.complete = g_championshipActive && g_championship.IsComplete();
    status.championId = g_championshipActive ? g_championship.ChampionId() : -1;
    return status;
}

StandingsView ApiStandings(SortMethod method)
{
    StandingsView view;
    view.rows.data = nullptr;
    view.rows.count = 0;
    view.comparisons = 0;
    view.swaps = 0;
    view.micros = 0.0;
    view.allMethodsAgree = false;

    if (!g_championshipActive)
    {
        SetError("no championship running");
        return view;
    }

    algo::AlgoStats stats;
    DynArray<model::ChampionshipEntry> sorted;
    double before = Timer::NowMicros();
    g_championship.Standings(ToSimSortMethod(method), &sorted, &stats);
    double after = Timer::NowMicros();

    FillStandingRows(sorted, &g_standingRows);
    view.rows.data = g_standingRows.Data();
    view.rows.count = g_standingRows.Size();
    view.comparisons = stats.comparisons;
    view.swaps = stats.swaps;
    view.micros = after - before;
    view.allMethodsAgree = g_championship.SelfCheck();
    return view;
}

TopKView ApiTopK(int k)
{
    TopKView view;
    view.rows.data = nullptr;
    view.rows.count = 0;
    view.comparisons = 0;
    view.fullSortComparisons = 0;
    view.matchesSortPrefix = false;

    if (!g_championshipActive)
    {
        SetError("no championship running");
        return view;
    }

    algo::AlgoStats heapStats;
    DynArray<model::ChampionshipEntry> top;
    g_championship.TopK(k, &top, &heapStats);

    algo::AlgoStats sortStats;
    DynArray<model::ChampionshipEntry> sorted;
    g_championship.Standings(sim::SortMethod::Merge, &sorted, &sortStats);

    bool matches = true;
    for (int i = 0; i < top.Size(); ++i)
    {
        if (top[i].racerId != sorted[i].racerId) matches = false;
    }

    FillStandingRows(top, &g_topKRows);
    view.rows.data = g_topKRows.Data();
    view.rows.count = g_topKRows.Size();
    view.comparisons = heapStats.comparisons;
    view.fullSortComparisons = sortStats.comparisons;
    view.matchesSortPrefix = matches;
    return view;
}

ResultsView ApiRaceHistory(int raceIndex)
{
    ResultsView view;
    view.rows.data = nullptr;
    view.rows.count = 0;

    if (!g_championshipActive)
    {
        SetError("no championship running");
        return view;
    }

    const model::RaceResultRow* rows = nullptr;
    int count = g_championship.RaceHistory(raceIndex, &rows);
    if (count == 0)
    {
        SetError("no such race in this championship yet");
        return view;
    }

    g_historyRows.Clear();
    for (int i = 0; i < count; ++i)
    {
        ResultRowView rv;
        rv.racerId = rows[i].racerId;
        rv.finishPosition = rows[i].finishPosition;
        rv.finishTimeMs = rows[i].finishTimeMs;
        rv.bestLapTimeMs = rows[i].bestLapTimeMs;
        rv.bonusPoints = rows[i].bonusPoints;
        rv.pointsAwarded = rows[i].pointsAwarded;
        rv.status = static_cast<int>(rows[i].status);
        rv.lapsCompleted = rows[i].lapsCompleted;
        g_historyRows.Push(rv);
    }

    view.rows.data = g_historyRows.Data();
    view.rows.count = g_historyRows.Size();
    return view;
}

TieBreak ApiTieBreakReason(int racerA, int racerB)
{
    switch (g_championship.TieBreak(racerA, racerB))
    {
        case algo::TieBreakReason::Points: return TieBreak::Points;
        case algo::TieBreakReason::Wins: return TieBreak::Wins;
        case algo::TieBreakReason::SumOfFinishPositions: return TieBreak::SumOfFinishPositions;
        case algo::TieBreakReason::BestFastestLap: return TieBreak::BestFastestLap;
        case algo::TieBreakReason::RacerId: return TieBreak::RacerId;
        case algo::TieBreakReason::Identical:
        default: return TieBreak::SameRacer;
    }
}

// ---------------------------------------------------------------------- //
// Track validation
// ---------------------------------------------------------------------- //

ValidationView ApiValidateTrack()
{
    ValidationView view;
    view.passed = false;
    view.checks.data = nullptr;
    view.checks.count = 0;

    if (!g_circuitLoaded)
    {
        SetError("no circuit loaded");
        return view;
    }

    g_validationViews.Clear();
    for (int i = 0; i < sim::kValidationCheckCount; ++i)
    {
        ValidationCheckView check;
        check.name = sim::ValidationCheckName(static_cast<sim::ValidationCheck>(i));
        check.passed = g_validation.check[i];
        check.detail = g_validation.detail[i];
        g_validationViews.Push(check);
    }

    view.passed = g_validation.passed;
    view.checks.data = g_validationViews.Data();
    view.checks.count = g_validationViews.Size();
    return view;
}

// ---------------------------------------------------------------------- //
// Internal accessors for Lab.cpp (see ApiInternal.h)
// ---------------------------------------------------------------------- //

namespace api_internal
{
    bool CircuitLoaded()
    {
        return g_circuitLoaded;
    }

    model::Graph* CurrentGraph()
    {
        return g_graph;
    }

    const io::TrackData& CurrentTrack()
    {
        return g_trackData;
    }

    int StartVertex()
    {
        return g_startVertex;
    }

    int FinishVertex()
    {
        return g_finishVertex;
    }

    const model::CostReferences& CostRefs()
    {
        return g_costReferences;
    }

    int Laps()
    {
        return g_settings.laps;
    }

    bool ChampionshipActive()
    {
        return g_championshipActive;
    }

    const sim::ChampionshipManager& Championship()
    {
        return g_championship;
    }

    void SetError(const char* message)
    {
        ::SetError(message);
    }

    model::Preset ToModelPreset(Preset preset)
    {
        return ::ToModelPreset(preset);
    }

    void BuildGraphViews(const model::Graph& graph, DynArray<VertexView>* vertices, DynArray<EdgeView>* edges)
    {
        BuildViewsOf(graph, vertices, edges);
    }
}
