// SmartRace backend API implementation.
//
// Session/Main Menu, static track data and the Track Explorer queries are
// REAL as of Phase 1 (TrackLoader + Graph + BFS/DFS/Dijkstra/A*/
// RoutePlanner). Racing (ApiRaceStart/Step/Snapshot) is still a stub: it
// drives a handful of fake racers along the loaded circuit's real MAINROUTE
// (so at least they move over real track geometry), not a real simulation.
// The real deterministic RaceEngine replaces that part in Phase 2 without
// changing these signatures.
#include "SmartRaceApi.h"
#include "../model/Rules.h"
#include "../model/CostModel.h"
#include "../io/TrackLoader.h"
#include "../algo/Bfs.h"
#include "../algo/Dfs.h"
#include "../algo/RoutePlanner.h"
#include "../core/util/String.h"
#include "../core/util/MathUtil.h"
#include "../core/ds/DynArray.h"

#include <cmath>
#include <cstdio>

namespace
{
    const int kMaxRacers = 4;

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

    model::Graph* g_graph = nullptr;
    String g_circuitName;
    int g_laps = Rules::kDefaultLaps;
    int g_startVertex = -1;
    int g_finishVertex = -1;
    model::CostReferences g_costReferences;
    DynArray<int> g_mainRoute;

    DynArray<VertexView> g_vertexViews;
    DynArray<EdgeView> g_edgeViews;

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

    // Racing stub state: fake racers driven along the loaded circuit's real
    // MAINROUTE vertex sequence (Phase 2 replaces this with the real engine).
    RacerView g_racers[kMaxRacers];
    int g_racerCount = 0;
    int g_racerRouteIndex[kMaxRacers];
    int g_raceTimeMs = 0;

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

    void RebuildViews()
    {
        g_vertexViews.Clear();
        for (int i = 0; i < g_graph->VertexCount(); ++i)
        {
            const model::Vertex& v = g_graph->GetVertex(i);
            VertexView vv;
            vv.id = v.id;
            vv.x = v.x;
            vv.y = v.y;
            vv.type = ToApiVertexType(v.type);
            vv.checkpointOrder = v.checkpointOrder;
            vv.active = v.active;
            g_vertexViews.Push(vv);
        }

        g_edgeViews.Clear();
        for (int i = 0; i < g_graph->EdgeCount(); ++i)
        {
            const model::Edge& e = g_graph->GetEdge(i);
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
            g_edgeViews.Push(ev);
        }
    }

    int ResolveEdgeBetween(int from, int to)
    {
        if (g_graph == nullptr) return -1;
        for (const model::AdjNode* node = g_graph->Neighbors(from); node != nullptr; node = node->next)
        {
            if (node->neighbor == to) return node->edgeId;
        }
        return -1;
    }

    void BuildStubRacers()
    {
        g_racerCount = (g_mainRoute.Size() >= 2) ? kMaxRacers : 0;
        static const char* const kNames[kMaxRacers] = { "You", "Racer A", "Racer B", "Racer C" };

        for (int i = 0; i < g_racerCount; ++i)
        {
            RacerView& racer = g_racers[i];
            racer.id = i;
            racer.name = kNames[i];
            racer.isHuman = (i == 0);
            racer.state = RacerState::OnEdge;
            racer.currentVertex = -1;
            racer.edgeProgress = 0.0;
            racer.fuelTenths = Rules::kMaxFuelTenths;
            racer.nitroCharges = g_settings.startNitroCharges;
            racer.nitroActive = false;
            racer.lap = 1;
            racer.nextCheckpoint = 1;
            racer.livePosition = i + 1;
            racer.totalTimeMs = 0;

            g_racerRouteIndex[i] = i % g_mainRoute.Size();
            int from = g_mainRoute[g_racerRouteIndex[i]];
            int to = g_mainRoute[(g_racerRouteIndex[i] + 1) % g_mainRoute.Size()];
            racer.currentEdge = ResolveEdgeBetween(from, to);

            const model::Vertex& v = g_graph->GetVertex(from);
            racer.x = v.x;
            racer.y = v.y;
            racer.heading = 0.0;
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
    SetError("");
    return ApiStatus::Ok;
}

void ApiShutdown()
{
    g_initialized = false;
    g_circuitLoaded = false;
    g_raceRunning = false;
    g_raceOver = false;
    delete g_graph;
    g_graph = nullptr;
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

    io::TrackData data;
    int errorLine = 0;
    char errorMessage[200];
    io::LoadResult loadResult = io::LoadTrackFile(path.CStr(), &data, &errorLine, errorMessage, sizeof(errorMessage));
    if (loadResult != io::LoadResult::Ok)
    {
        char full[320];
        snprintf(full, sizeof(full), "%s (line %d): %s", path.CStr(), errorLine, errorMessage);
        SetError(full);
        return ApiStatus::IoError;
    }

    delete g_graph;
    g_graph = data.graph;
    data.graph = nullptr;   // ownership transferred to g_graph; TrackData won't double-free it

    g_circuitName.Set(data.name.Empty() ? kCircuits[index].fallbackName : data.name.CStr());
    g_laps = data.laps;

    g_mainRoute.Clear();
    for (int i = 0; i < data.mainRoute.Size(); ++i) g_mainRoute.Push(data.mainRoute[i]);

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

    g_circuitLoaded = true;
    g_raceRunning = false;
    g_raceOver = false;
    return ApiStatus::Ok;
}

ApiStatus ApiLoadCircuitFile(const char* path)
{
    (void)path;
    SetError("ApiLoadCircuitFile is not implemented until Phase 4 (track generator)");
    return ApiStatus::IoError;
}

Settings ApiGetSettings()
{
    return g_settings;
}

ApiStatus ApiSetSettings(const Settings& settings)
{
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

    // The real TrackValidator lands in Phase 4; until then we report the
    // structural checks we already have (connectivity is implied by
    // Start/Finish both being set and the circuit having loaded at all).
    info.validatorPassed = (g_startVertex >= 0 && g_finishVertex >= 0 && cpCount >= Rules::kMandatoryCheckpointsPerCircuit);

    return info;
}

ApiStatus ApiRaceStart()
{
    if (!g_circuitLoaded)
    {
        SetError("no circuit loaded");
        return ApiStatus::NotLoaded;
    }
    if (g_mainRoute.Size() < 2)
    {
        SetError("circuit has no MAINROUTE to drive the stub racers along");
        return ApiStatus::Infeasible;
    }

    BuildStubRacers();
    g_raceRunning = true;
    g_raceOver = false;
    g_raceTimeMs = 0;
    return ApiStatus::Ok;
}

void ApiRaceStep(int ticks)
{
    if (!g_raceRunning || g_raceOver || g_mainRoute.Size() < 2) return;

    int routeLen = g_mainRoute.Size();

    for (int t = 0; t < ticks; ++t)
    {
        g_raceTimeMs += Rules::kSimStepMs;

        for (int i = 0; i < g_racerCount; ++i)
        {
            RacerView& racer = g_racers[i];
            if (racer.state != RacerState::OnEdge) continue;

            double speed = 60.0 + 10.0 * i;   // world units/sec, distinct per racer
            int from = g_mainRoute[g_racerRouteIndex[i]];
            int to = g_mainRoute[(g_racerRouteIndex[i] + 1) % routeLen];

            const model::Vertex& vFrom = g_graph->GetVertex(from);
            const model::Vertex& vTo = g_graph->GetVertex(to);
            double segLength = MathUtil::Dist2D(vFrom.x, vFrom.y, vTo.x, vTo.y);
            double step = (segLength > 0.0) ? (speed * (Rules::kSimStepMs / 1000.0) / segLength) : 1.0;

            racer.edgeProgress += step;
            if (racer.edgeProgress >= 1.0)
            {
                racer.edgeProgress = 0.0;
                g_racerRouteIndex[i] = (g_racerRouteIndex[i] + 1) % routeLen;
                from = g_mainRoute[g_racerRouteIndex[i]];
                to = g_mainRoute[(g_racerRouteIndex[i] + 1) % routeLen];
                racer.currentEdge = ResolveEdgeBetween(from, to);
            }

            const model::Vertex& a = g_graph->GetVertex(from);
            const model::Vertex& b = g_graph->GetVertex(to);
            racer.x = a.x + (b.x - a.x) * racer.edgeProgress;
            racer.y = a.y + (b.y - a.y) * racer.edgeProgress;
            racer.heading = atan2(b.y - a.y, b.x - a.x);
            racer.totalTimeMs = g_raceTimeMs;
        }
    }
}

RaceSnapshot ApiRaceSnapshot()
{
    RaceSnapshot snapshot;
    snapshot.timeMs = g_raceTimeMs;
    snapshot.racers.data = g_racers;
    snapshot.racers.count = g_raceRunning ? g_racerCount : 0;
    snapshot.raceOver = g_raceOver;
    return snapshot;
}

bool ApiRaceOver()
{
    return g_raceOver;
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
