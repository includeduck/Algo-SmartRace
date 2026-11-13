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

// ---------------------------------------------------------------------- //
// Track Explorer / Route Planner (Screen 2) — added in Phase 1
// ---------------------------------------------------------------------- //

struct ReachabilityView
{
    bool found;                           // meaningful only when a target >= 0 was given
    ArrayView<int> path;                    // source..target, only set when found
    ArrayView<bool> reachable;               // per-vertex, sized to the graph's vertex count
    ArrayView<int> reachableCheckpoints;      // mandatory checkpoint ORDER numbers reachable
};

struct ComponentsView
{
    int componentCount;
    ArrayView<int> componentId;   // per-vertex, 0-based
};

struct CycleView
{
    bool hasCycle;
    ArrayView<int> witnessCycle;   // a vertex sequence forming one cycle, if hasCycle
};

struct DeadEndsView
{
    ArrayView<int> deadEndVertices;
};

struct RouteView
{
    bool found;
    int blockedLegIndex;      // -1 if found, else the first leg (toward a checkpoint/finish) that failed
    ArrayView<int> vertexPath;
    ArrayView<int> edgePath;
    double totalCost, totalDistance, totalTime, totalFuel;
};

struct RouteCandidateView
{
    const char* label;   // e.g. "Balanced", "Speed", "Alternative 1"
    bool found;
    ArrayView<int> vertexPath;
    double totalCost, totalDistance, totalTime, totalFuel;   // all under the comparison's evalPreset
};

struct RouteCompareView
{
    ArrayView<RouteCandidateView> candidates;
};

// ---------------------------------------------------------------------- //
// Racing results and event log — added in Phase 2
// ---------------------------------------------------------------------- //

struct ResultRowView
{
    int racerId;
    int finishPosition;
    int finishTimeMs;       // -1 if did not finish
    int bestLapTimeMs;        // INT_MAX if no lap completed
    int bonusPoints;
    int pointsAwarded;
    int status;                 // 0 = Finished, 1 = Dnf, 2 = TimeLimit (mirrors model::RaceStatus)
    int lapsCompleted;
};

struct ResultsView
{
    ArrayView<ResultRowView> rows;
};

struct LogLineView
{
    int index;
    int timeMs;
    const char* text;
};

struct LogView
{
    ArrayView<LogLineView> lines;
};
