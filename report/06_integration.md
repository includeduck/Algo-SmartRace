# 6. Integration: Where Each Algorithm Runs in the Game

*Backend section — Muhammad Wasay Tariq (24I-3071). The "Screen" column refers to Syed Muhammad Rayyan Hassan's GUI chapter (24I-3000).*

The brief's design principle is that every mandatory algorithm has a functional role in the game, and that the Algorithm Lab calls the **same implementation** the game uses. Both properties come from the architecture:

- **One library.** The GUI, the tests, the benchmark program and the CLI all link the same static library `smartrace_core`. The GUI reaches it only through `src/api/SmartRaceApi.h`. It never searches, plans, sorts or computes a cost itself.
- **One function, two modes.** Every algorithm takes an optional `Trace*` recorder. Gameplay passes `nullptr`. The Lab (`src/api/Lab.cpp`) calls the identical function once with a recorder attached, and `TraceCursor` then rebuilds the visible state after any recorded step, forwards or backwards. There is no separate "demo" copy of any algorithm, so the Lab can't drift away from the game.

| # | Algorithm | Gameplay use (call site) | API entry point | Screen | Tests | Exp |
|---|---|---|---|---|---|---|
| 1 | BFS | Explorer reachability and fewest-road path (`ApiRunBfs`); validator: every checkpoint leg reachable in order (`TrackValidator`); generator: connectivity of placed roles | `ApiRunBfs`, `ApiValidateTrack` | Explorer | 4 + validator | 1 |
| 2 | DFS | components, cycle witness and dead ends in the Explorer; **bounded alternative routes** in the route comparison (`CompareRoutes`); validator (cycle, dead end, alternative route); generator (candidate connectivity, **bridges** so obstacles never cut the track) | `ApiRunDfsComponents`, `ApiRunDfsCycles`, `ApiRunDeadEnds`, `ApiCompareRoutes` | Explorer | 9 | 1 |
| 3 | Dijkstra | every opponent-3 route; every refuelling detour and the knapsack's detour costs (`ResourcePlanner`); preset routes in the comparison; Explorer lap plan | `ApiPlanLap(…, Dijkstra)`, `ApiCompareRoutes`, race engine | Explorer, Racing | 5 + planner | 2 |
| 4 | A* | **the human autopilot's in-race router** (`RaceEngine::ReplanRoute`, re-run after every graph change); Explorer lap plan option | `ApiPlanLap(…, AStar)`, race engine | Explorer, Racing | 3 | 2 |
| 5 | Dynamic programming | knapsack: optional pickups within a detour budget; refuelling DP: the autopilot's purchase at each station, the refuel pop-up's recommendation, the Explorer's refuel plan, the validator's "feasible fuel plan" check | `ApiPlanPickups`, `ApiPlanRefuel`, `ApiRecommendedRefuel` | Explorer, Racing | 13 | 3 |
| 6 | Greedy | ratio heuristic shown next to the DP's answer for the same pickups | `ApiPlanPickups` | Explorer | 3 | 3 |
| 7 | Backtracking | places Start, CP1..10, Finish, stations, nitro, then obstacles, in the track generator; the saved track becomes playable | `ApiLabRun(TrackGenerator)` → `ApiLabSaveGenerated` → `ApiLoadCircuitFile` | Lab → Explorer/Racing | 4 | — |
| 8 | Kruskal (+ union-find) | MST backbone of every generated track | `ApiLabRun(TrackGenerator)`, `ApiLabRun(Kruskal)` | Lab | 3 | — |
| 9 | Heap / priority queue | race event scheduling (every scripted event, every expiry, cancellations); the Dijkstra/A* frontier; live top-k standings | race engine, `ApiTopK` | Racing, Championship | 5 + DS | 4, 5 |
| 10 | Merge Sort | championship standings (default); every race's finish order; the greedy's ratio order; Kruskal's edge order; benchmark medians | `ApiStandings(Merge)`, `ApiRaceResults` | Results, Championship | 7 | 5 |
| 11 | Quick Sort | alternative standings method | `ApiStandings(Quick)` | Championship | 7 | 5 |
| 12 | Heap Sort | third standings method | `ApiStandings(Heap)` | Championship | 7 | 5 |

Every row is also a module of the Algorithm Lab (`LabAlgo`): BFS, DFS, Dijkstra, A*, Kruskal, Track Generator, Knapsack DP, Greedy, Refuel DP, Merge/Quick/Heap Sort and Event Queue. The Lab runs on the **current** data: the loaded circuit including live race effects (so a road blocked mid-race shows up in the Lab's Dijkstra), and the live championship standings for the sorts.

## 6.1 The race loop: what runs when

Each 20 ms tick (`RaceEngine::Step`):

1. **Event heap.** `ProcessUntil(now)` pops every due event in (time, priority, sequence) order and applies each through the effect manager. Timed effects schedule their own expiries; collecting a nitro booster cancels its pending expiry (lazy deletion).
2. **Re-planning.** If any popped event changed the graph, every racer is marked for re-planning, **once per batch** (the brief: apply all simultaneous events before recalculating). Opponents re-plan with their strategy (fixed route + documented detour, shortcut preference, or Dijkstra on distance); the human re-plans with A* under their preset. A failed plan puts the racer into the recovery policy (wait, re-plan on each change, DNF after 60 s).
3. **Fuel decisions.** A racer below 25% fuel, or short of fuel for its remaining route, is routed to a station (`ResourcePlanner::PlanRefuelDetour`, Dijkstra). At the station, opponents buy their strategy's amount and the autopilot human buys the **refuelling DP's** recommendation.
4. **Movement.** Each racer advances along its edge at the effective speed (difficulty, speed caps, nitro), burning fuel by the edge rule. Arriving at a vertex updates checkpoint progress, lap count and pickups.
5. **End of race.** When the race ends (all done, or 60 s after the leader, or 15 min), the results are ordered by **Merge Sort** with the finish comparator, and points are awarded.

Between races, `ChampionshipManager` adds the results to the standings and ranks them with whichever sort the dashboard asks for. It also verifies that all three sorts agree and answers top-k queries with the size-k heap.

## 6.2 What the GUI receives

The backend hands every screen ready-to-draw values. A few examples: the scalar cost each route optimised, per-vertex BFS reachability, the DP's chosen items next to the greedy's, the refuel recommendation, the event log, the live running order, the sort's comparison and swap counts, top-k with its "matches the sorted prefix" flag, and Lab frames with per-vertex, per-edge and per-cell state plus the current pseudocode line. The API was frozen at v1.0; later versions only fixed bugs (v1.0.1, v1.0.2) or added functions and fields (v1.1, which answered the GUI's requests for ordered race-lap comparison, routes between any two vertices, per-road costs, live-state resource planning in the Lab and a structured Lab data source). Its rules — borrowed views, status codes with readable errors, single-threaded, time only advances through `ApiRaceStep` — are documented at the top of `SmartRaceApi.h`.
