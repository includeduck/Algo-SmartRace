// SmartRace backend API — v0.1 STUB implementation.
//
// Everything below is a placeholder so the GUI (Rayyan) can link against
// the real header and draw something from day one. It hard-codes a tiny
// hexagonal circuit and moves a handful of fake racers around it at fixed
// speeds. None of this is a real algorithm or a real race: Phase 1 replaces
// the track with TrackLoader + Graph, and Phase 2 replaces the racing stub
// with the real deterministic RaceEngine. The function signatures in
// SmartRaceApi.h do not change when that happens.
#include "SmartRaceApi.h"
#include "../model/Rules.h"

#include <cmath>

namespace
{
    const int kMaxVertices = 8;
    const int kMaxEdges = 8;
    const int kMaxRacers = 4;
    const double kPi = 3.14159265358979323846;

    bool g_initialized = false;
    bool g_circuitLoaded = false;
    bool g_raceRunning = false;
    bool g_raceOver = false;

    char g_lastError[256] = "";

    Settings g_settings =
    {
        Rules::kDefaultLaps,
        3,                                     // opponentCount
        Preset::Balanced,
        Rules::kDefaultStartNitroCharges,
        42ULL,                                  // eventSeed
        true,                                    // showCostLabels
    };

    VertexView g_vertices[kMaxVertices];
    EdgeView g_edges[kMaxEdges];
    int g_vertexCount = 0;
    int g_edgeCount = 0;

    RacerView g_racers[kMaxRacers];
    int g_racerCount = 0;
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

    // Builds a tiny hard-coded hexagonal loop so there is something real to
    // draw and drive before TrackLoader/Graph exist (Phase 1).
    void BuildStubCircuit()
    {
        g_vertexCount = 6;
        const VertexType types[6] =
        {
            VertexType::Start, VertexType::Checkpoint, VertexType::Checkpoint,
            VertexType::Finish, VertexType::FuelStation, VertexType::NitroPickup,
        };
        const int cpOrder[6] = { -1, 1, 2, -1, -1, -1 };

        for (int i = 0; i < g_vertexCount; ++i)
        {
            double angle = (2.0 * kPi * i) / g_vertexCount;
            g_vertices[i].id = i;
            g_vertices[i].x = 200.0 + 150.0 * cos(angle);
            g_vertices[i].y = 200.0 + 150.0 * sin(angle);
            g_vertices[i].type = types[i];
            g_vertices[i].checkpointOrder = cpOrder[i];
            g_vertices[i].active = true;
        }

        g_edgeCount = g_vertexCount;
        for (int i = 0; i < g_edgeCount; ++i)
        {
            int a = i;
            int b = (i + 1) % g_vertexCount;

            g_edges[i].id = i;
            g_edges[i].from = a;
            g_edges[i].to = b;

            double dx = g_vertices[b].x - g_vertices[a].x;
            double dy = g_vertices[b].y - g_vertices[a].y;
            g_edges[i].distance = sqrt(dx * dx + dy * dy);
            g_edges[i].baseTime = g_edges[i].distance / 40.0;
            g_edges[i].fuelCost = 5.0;
            g_edges[i].difficulty = 1;
            g_edges[i].effDifficulty = 1;
            g_edges[i].status = EdgeStatus::Open;
            g_edges[i].speedCap = -1.0;
            g_edges[i].isShortcut = false;
            g_edges[i].directed = false;
        }
    }

    void BuildStubRacers()
    {
        g_racerCount = 4;
        static const char* const names[4] = { "You", "Racer A", "Racer B", "Racer C" };

        for (int i = 0; i < g_racerCount; ++i)
        {
            RacerView& racer = g_racers[i];
            racer.id = i;
            racer.name = names[i];
            racer.isHuman = (i == 0);
            racer.state = RacerState::OnEdge;
            racer.currentEdge = i % g_edgeCount;
            racer.edgeProgress = 0.0;
            racer.currentVertex = -1;
            racer.fuelTenths = Rules::kMaxFuelTenths;
            racer.nitroCharges = g_settings.startNitroCharges;
            racer.nitroActive = false;
            racer.lap = 1;
            racer.nextCheckpoint = 1;
            racer.livePosition = i + 1;
            racer.totalTimeMs = 0;

            const EdgeView& edge = g_edges[racer.currentEdge];
            racer.x = g_vertices[edge.from].x;
            racer.y = g_vertices[edge.from].y;
            racer.heading = 0.0;
        }
    }
}

ApiStatus ApiInit(const char* dataDir)
{
    (void)dataDir;   // v0.1 stub does not read any files yet (Phase 1 adds TrackLoader)
    g_initialized = true;
    g_circuitLoaded = false;
    g_raceRunning = false;
    g_raceOver = false;
    SetError("");
    return ApiStatus::Ok;
}

void ApiShutdown()
{
    g_initialized = false;
    g_circuitLoaded = false;
    g_raceRunning = false;
    g_raceOver = false;
}

const char* ApiLastError()
{
    return g_lastError;
}

int ApiCircuitCount()
{
    if (!g_initialized) return 0;
    return 1;   // v0.1: one stub circuit. Phase 1 reads data/circuits/*.trk.
}

const char* ApiCircuitName(int index)
{
    if (index != 0) return "";
    return "Stub Circuit (placeholder — Phase 1 adds the 3 real circuits)";
}

ApiStatus ApiLoadCircuit(int index)
{
    if (!g_initialized)
    {
        SetError("ApiInit was not called");
        return ApiStatus::NotInitialized;
    }
    if (index != 0)
    {
        SetError("no such circuit index");
        return ApiStatus::InvalidId;
    }

    BuildStubCircuit();
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
    view.vertices.data = g_vertices;
    view.vertices.count = g_circuitLoaded ? g_vertexCount : 0;
    view.edges.data = g_edges;
    view.edges.count = g_circuitLoaded ? g_edgeCount : 0;
    return view;
}

TrackInfo ApiGetTrackInfo()
{
    TrackInfo info;
    info.name = g_circuitLoaded ? "Stub Circuit (placeholder)" : "";
    info.vertexCount = g_circuitLoaded ? g_vertexCount : 0;
    info.edgeCount = g_circuitLoaded ? g_edgeCount : 0;
    info.mandatoryCheckpointCount = 2;
    info.shortcutCount = 0;
    info.laps = g_settings.laps;
    info.hasCycle = true;
    info.hasDeadEnd = false;
    info.validatorPassed = g_circuitLoaded;
    return info;
}

ApiStatus ApiRaceStart()
{
    if (!g_circuitLoaded)
    {
        SetError("no circuit loaded");
        return ApiStatus::NotLoaded;
    }

    BuildStubRacers();
    g_raceRunning = true;
    g_raceOver = false;
    g_raceTimeMs = 0;
    return ApiStatus::Ok;
}

void ApiRaceStep(int ticks)
{
    if (!g_raceRunning || g_raceOver) return;

    for (int t = 0; t < ticks; ++t)
    {
        g_raceTimeMs += Rules::kSimStepMs;

        for (int i = 0; i < g_racerCount; ++i)
        {
            RacerView& racer = g_racers[i];
            if (racer.state != RacerState::OnEdge) continue;

            double speed = 0.15 + 0.03 * i;    // distinct stub speeds per racer
            racer.edgeProgress += speed * (Rules::kSimStepMs / 1000.0);
            if (racer.edgeProgress >= 1.0)
            {
                racer.edgeProgress = 0.0;
                racer.currentEdge = (racer.currentEdge + 1) % g_edgeCount;
            }

            const EdgeView& edge = g_edges[racer.currentEdge];
            const VertexView& from = g_vertices[edge.from];
            const VertexView& to = g_vertices[edge.to];
            racer.x = from.x + (to.x - from.x) * racer.edgeProgress;
            racer.y = from.y + (to.y - from.y) * racer.edgeProgress;
            racer.heading = atan2(to.y - from.y, to.x - from.x);
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
