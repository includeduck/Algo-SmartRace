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

// ---------------------------------------------------------------------- //
// Resource planning (Explorer "Resources" tab) — added in Phase 3
// ---------------------------------------------------------------------- //

struct PickupItemView
{
    int vertex;
    bool isNitro;              // false = fuel station
    int costTenthsSec;          // detour time (+ stop time for fuel), tenths of a second
    int benefitTenthsSec;        // estimated time saved / penalty avoided, tenths of a second
    double detourSeconds;
    bool chosenByDp;
    bool chosenByGreedy;
};

struct PickupView
{
    ArrayView<PickupItemView> items;
    int budgetTenthsSec;
    int dpBenefit, dpCost;
    int greedyBenefit, greedyCost;
    long long dpStates;        // table cells filled, for the "why DP costs O(mB)" display
    bool fuelNeeded;             // true when the race's refuel plan is infeasible without a detour
};

struct RefuelStopView
{
    int vertex;
    bool canBuy;            // an active station (stop 0 is where the racer stands)
    int purchaseTenths;      // recommended purchase, 0 = drive through
};

struct RefuelView
{
    bool feasible;
    int totalStopTimeMs;
    int finalFuelTenths;
    int stopCount;
    ArrayView<RefuelStopView> stops;
};

// ---------------------------------------------------------------------- //
// Championship dashboard (Screen 5) — added in Phase 3
// ---------------------------------------------------------------------- //

struct StandingRowView
{
    int position;
    int racerId;
    const char* name;
    int totalPoints;
    int wins;
    int sumOfFinishPositions;
    int bestFastestLapMs;     // INT_MAX if no lap completed yet
};

struct StandingsView
{
    ArrayView<StandingRowView> rows;
    long long comparisons;     // for the method that produced `rows`
    long long swaps;
    double micros;
    bool allMethodsAgree;       // Merge == Quick == Heap
};

struct TopKView
{
    ArrayView<StandingRowView> rows;
    long long comparisons;          // size-k heap
    long long fullSortComparisons;    // Merge Sort of all entries, for comparison
    bool matchesSortPrefix;
};

struct ChampionshipStatus
{
    bool active;
    int racesCompleted;
    int racesTotal;
    int currentCircuitIndex;     // circuit of the race to run next (or the last one, once complete)
    bool complete;
    int championId;               // -1 until complete
};

// Which criterion separates two racers in the standings (ApiTieBreakReason).
enum class TieBreak
{
    Points,
    Wins,
    SumOfFinishPositions,
    BestFastestLap,
    RacerId,
    SameRacer,
};

// ---------------------------------------------------------------------- //
// Track validation and generation — added in Phase 4
// ---------------------------------------------------------------------- //

struct ValidationCheckView
{
    const char* name;
    bool passed;
    const char* detail;    // why it failed ("" when it passed)
};

struct ValidationView
{
    bool passed;
    ArrayView<ValidationCheckView> checks;
};

struct GeneratorAttemptView
{
    int attempt;
    bool accepted;
    const char* reason;            // why the attempt was rejected ("" if accepted)
    long long nodesExplored;         // backtracking assignments tried
    long long prunedBranches;          // candidates cut by a pruning rule
};

struct GeneratorReportView
{
    ArrayView<GeneratorAttemptView> attempts;
    int acceptedAttempt;   // -1 if every attempt failed
};

// ---------------------------------------------------------------------- //
// Algorithm Lab (Screen 4) — added in Phase 4
// ---------------------------------------------------------------------- //

enum class LabAlgo
{
    Bfs,
    Dfs,
    Dijkstra,
    AStar,
    Kruskal,
    TrackGenerator,
    KnapsackDp,
    GreedyPickup,
    RefuelDp,
    MergeSort,
    QuickSort,
    HeapSort,
    EventQueue,
};

// How to read a LabFrame (which of its arrays are meaningful).
enum class LabFrameKind
{
    Graph,    // vertexState/vertexValue/vertexParent/edgeState over ApiLabGraph()
    Array,     // arrayValues: the array being sorted, as element keys
    Table,      // tableCells (rows x cols) + selectedIds / rejectedIds
    Events,      // selectedIds = event ids popped in order, rejectedIds = cancelled
};

// The operation a step performed (same order as the backend's TraceOp).
enum class LabOp
{
    VisitVertex,
    DiscoverVertex,
    FinishVertex,
    ExamineEdge,
    RelaxEdge,
    PushHeap,
    PopHeap,
    SetParent,
    DpCell,
    Compare,
    Swap,
    Place,
    Backtrack,
    Reject,
    UnionSets,
    AcceptEdge,
    MarkEdge,
};

// Inputs for ApiLabRun. Each algorithm reads only the fields it needs;
// anything else is ignored. Start from ApiLabDefaultInput().
struct LabInput
{
    int source;                     // BFS/DFS/Dijkstra/A*; -1 = the track's Start
    int target;                      // BFS/Dijkstra/A*; -1 = the mid-lap checkpoint (CP5), else the Finish
    Preset preset;                     // Dijkstra/A*/knapsack/refuel route
    int budgetTenthsSec;                 // knapsack/greedy budget
    bool useBriefExample;                  // knapsack/greedy: the brief's A/B/C items, budget 10
    int arraySize;                           // sorts without a championship: random keys (2..64)
    unsigned long long seed;                   // sorts' random keys; the generator's seed
    int generatorVertexCount;                    // generator: vertices to place
};

// The algorithm's visible state after one step. Vertex/edge codes for the
// Graph kind:
//   vertexState: 0 unseen, 1 discovered / in the frontier, 2 visited /
//                settled, 3 finished (DFS). For the generator instead a role:
//                1 Start, 2 Finish, 3 Checkpoint (vertexValue = its order),
//                4 Fuel station, 5 Nitro pickup.
//   vertexValue: BFS hop distance, DFS discovery time, Dijkstra/A* cost so
//                far; -1 = none yet.
//   edgeState:   0 untouched, 1 examined, 2 relaxed / tree road, 3 rejected
//                (would close a cycle); generator extras: 4 extra road,
//                5 shortcut, 6 oil spill, 7 road block.
// The arrays point into the Lab's own storage: valid until the next
// ApiLabFrame / ApiLabRun call.
struct LabFrame
{
    int step;                // -1 = before the first step
    int stepCount;
    LabFrameKind kind;

    LabOp op;                 // this step's operation (meaningless at step -1)
    int a;
    int b;
    double value;
    int line;                  // index into ApiLabPseudocode() lines, -1 = none

    ArrayView<int> vertexState;
    ArrayView<double> vertexValue;
    ArrayView<int> vertexParent;
    ArrayView<int> edgeState;

    ArrayView<int> arrayValues;

    int tableRows;
    int tableCols;
    ArrayView<int> tableCells;   // row-major, -1 = not filled yet
    ArrayView<int> selectedIds;
    ArrayView<int> rejectedIds;

    // Running totals up to and including this step.
    long long visited;
    long long examined;
    long long relaxed;
    long long comparisons;
    long long swaps;
    long long tableCellsFilled;
    long long heapPushes;
    long long heapPops;
    long long placements;
    long long backtracks;
    long long rejections;
};

struct PseudocodeView
{
    ArrayView<const char*> lines;
};

// One event the EventQueue lab scheduled (ids match LabFrame's event ids).
struct LabEventView
{
    int id;
    const char* type;
    int triggerTimeMs;
    int priority;
    int sequence;
    int target;
};
