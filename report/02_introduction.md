# 2. Introduction and Problem Statement

A shortest road is not always the best racing decision. Travel time, fuel,
difficulty, mandatory checkpoints and temporary road restrictions can change
which route is useful. Smart Racing Championship turns these trade-offs into
a deterministic, two-dimensional top-down racing application.

The player competes with at least three rule-based opponents on three graph
circuits. Each circuit contains ten ordered checkpoints, alternative roads,
shortcuts, fuel stations, nitro pickups and authored events. The human controls
road-level decisions or delegates driving to the autopilot. This is strategic
graph-based racing; it does not implement continuous steering physics.

## 2.1 Problem statement

The application must let a player explore a road network, compare legal route
plans, manage distinct fuel and nitro resources, react to graph changes, and
review race and championship outcomes. A valid lap must preserve checkpoint
order. A road closure must trigger updated planning without restarting the
race. Every ranking view must use the same total comparator.

The educational requirement is equally important: the mandatory algorithms
must have functional roles in gameplay. The Algorithm Lab therefore invokes
the same backend implementations and replays their recorded operations. It
does not use a second collection of demonstration algorithms.

## 2.2 Objectives

- Make graph structure and the difference between minimum-road-count and
  weighted routing visible.
- Expose distance, estimated time, fuel and the exact optimized scalar cost.
- Compare DP resource recommendations with the feasible greedy heuristic.
- Demonstrate deterministic event scheduling, independent effect expiration,
  checkpoint-aware replanning and rule-based opponents.
- Display consistent rankings using Merge, Quick and Heap Sort, and a heap
  top-k query.
- Measure five experimental studies with reproducible inputs, raw timings and
  operation counters, separately from rendering.
- Provide a native executable, source, dependencies, documentation and one
  combined technical report.

## 2.3 Scope and assumptions

The game is an offline, single-threaded C++17 application for Windows x64.
The GUI uses raylib and raygui; the backend has its own containers and
algorithm implementations. There is no HTTP server, online multiplayer or
machine-learning opponent. Events follow the circuit's authored script; the
stored event seed is reserved for future event generation.

Route objectives are combined into a documented nonnegative weighted scalar.
Different presets may produce different routes; the GUI displays the backend
answer and never assumes that A* always expands fewer vertices. Resource
recommendations remain constrained by the configured race rules.

