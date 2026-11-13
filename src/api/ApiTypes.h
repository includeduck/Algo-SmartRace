#pragma once

// ===========================================================================
// SmartRace backend API — shared types.
//
// This header, together with SmartRaceApi.h, is the ONLY backend header the
// GUI (owned by Rayyan, src/app/) includes. Every type here is a plain
// struct, enum or primitive — no backend container types, no STL — so the
// GUI can be built with any toolkit (raylib or otherwise) and pass these
// values around freely.
// ===========================================================================

enum class ApiStatus
{
    Ok = 0,
    NotInitialized,
    NotLoaded,
    InvalidId,
    InvalidArgument,
    Illegal,
    Infeasible,
    IoError,
};

enum class VertexType
{
    Start,
    Finish,
    Checkpoint,
    Intersection,
    FuelStation,
    NitroPickup,
    DeadEnd,
};

enum class EdgeStatus
{
    Open,
    Blocked,
    Restricted,
};

enum class Preset
{
    Balanced,
    Speed,
    Economy,
    Safety,
};

enum class RouteAlgo
{
    Dijkstra,
    AStar,
};

enum class SortMethod
{
    Merge,
    Quick,
    Heap,
};

enum class RacerState
{
    AtVertex,
    OnEdge,
    Refuelling,
    Waiting,
    Finished,
    Dnf,
};

struct VertexView
{
    int id;
    double x, y;
    VertexType type;
    int checkpointOrder;   // -1 if this vertex is not a mandatory checkpoint
    bool active;             // false for a deactivated service vertex
};

struct EdgeView
{
    int id;
    int from, to;
    double distance, baseTime, fuelCost;
    int difficulty;          // base, 1..5
    int effDifficulty;       // base + active effects
    EdgeStatus status;        // effective status (base + active effects)
    double speedCap;          // <= 0 means no active cap
    bool isShortcut;
    bool directed;
};

// A borrowed, read-only view over a backend-owned array. Valid until the
// next call documented to invalidate it (usually: the next call that can
// change that data). The GUI copies out anything it needs to keep longer.
template <typename T>
struct ArrayView
{
    const T* data;
    int count;

    const T& operator[](int i) const
    {
        return data[i];
    }
};

struct GraphView
{
    ArrayView<VertexView> vertices;
    ArrayView<EdgeView> edges;
};

struct TrackInfo
{
    const char* name;
    int vertexCount;
    int edgeCount;
    int mandatoryCheckpointCount;
    int shortcutCount;
    int laps;
    bool hasCycle;
    bool hasDeadEnd;
    bool validatorPassed;
};

struct Settings
{
    int laps;                  // 2 or 3
    int opponentCount;          // 3..9
    Preset defaultPreset;
    int startNitroCharges;
    unsigned long long eventSeed;
    bool showCostLabels;
};

struct RacerView
{
    int id;
    const char* name;
    bool isHuman;
    RacerState state;
    double x, y;
    double heading;            // radians; GUI orients the car sprite with it
    int currentVertex;         // valid when state is AtVertex / Waiting / Refuelling
    int currentEdge;            // valid when state is OnEdge, -1 otherwise
    double edgeProgress;        // 0..1 along currentEdge
    int fuelTenths;              // 0..1000 (100.0 units)
    int nitroCharges;            // 0..3
    bool nitroActive;
    int lap;
    int nextCheckpoint;          // checkpoint order this racer must reach next
    int livePosition;            // 1-based current running order
    int totalTimeMs;
};

struct RaceSnapshot
{
    int timeMs;
    ArrayView<RacerView> racers;
    bool raceOver;
};
