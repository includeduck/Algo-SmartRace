# 4. Data Model, Cost Normalisation and Game Rules

*Backend section — Muhammad Wasay Tariq (24I-3071)*

## 4.1 Track graph

Each circuit is a graph G = (V, E) stored as **adjacency lists** (`src/model/Graph.h`). A vertex keeps a singly linked list of the edge ids that leave it. An undirected road appears in both endpoints' lists, a one-way road only in its tail's list. This uses O(V + E) memory and gives O(deg(v)) neighbour iteration, which suits our sparse circuits (21–26 vertices, 29–34 roads, average degree ≈ 2.7–3.2). An adjacency matrix would need O(V²) memory, most of it zeros.

| Vertex field | Meaning |
|---|---|
| `id` | index in the vertex array (so lookup is O(1)) |
| `x, y` | map coordinates (world units); also used by the A* heuristic |
| `type` | Start, Finish, Checkpoint, Intersection, FuelStation, NitroPickup, DeadEnd |
| `checkpointOrder` | 1..10 for mandatory checkpoints, −1 otherwise |
| `active` | whether a service vertex (station, nitro pickup) is currently available |

| Edge field | Meaning |
|---|---|
| `id` | index in the edge array; events refer to edges by this id |
| `from, to` | endpoints |
| `distance`, `baseTime`, `fuelCost` | non-negative base attributes |
| `difficulty` | 1 (easy) … 5 (hard), base value |
| `isShortcut`, `directed` | as in the brief |
| `effDifficulty`, `status`, `speedCap`, `obstaclePenalty` | **effective** state, recomputed by the effect manager from all active effects |

**Dynamic updates without rebuilding.** Blocking a road, restricting it or toggling a station only rewrites that edge's or vertex's effective fields (`Graph::SetEdgeStatus`, `EffectManager`). The adjacency lists never change. Every search skips edges whose effective status is `Blocked`, and restricted edges stay traversable at a higher cost.

**Other records** (`src/model/Records.h`, `Racer.h`): `Racer` (id, name, isHuman, strategy, current vertex/edge, progress, fuel, nitro, lap, next checkpoint, times, planned path), `RaceResultRow` (position, finish time, best lap, status, laps, points, bonus), `ChampionshipEntry` (racer id, points, wins, sum of finish positions, best fastest lap) and `Event` (id, type, trigger time, priority, insertion sequence, target, duration, cancelled/fired flags).

**Three hand-made circuits** (`data/circuits/*.trk`, authored with `tools/gen_circuits.py`):

| Circuit | V | E | Checkpoints | Shortcuts | Services | Scripted events |
|---|---|---|---|---|---|---|
| C1 Harbor Loop | 22 | 29 | 10 | 2 | 2 fuel, 2 nitro | oil spill 15 s, road block 40 s, reopen 55 s, nitro booster 70 s |
| C2 Twin Peaks | 26 | 34 | 10 | 2 | 2 fuel, 2 nitro | speed restriction 20 s, station outage 35 s, road block 50 s, reopen 70 s |
| C3 Downtown | 21 | 34 | 10 | 2 | 2 fuel, 2 nitro | oil spill 10 s, road block 45 s, reopen 65 s |

Every circuit has at least one cycle and one dead end, and passes all 11 validator checks (section 5.10). Every road's length equals the straight-line distance between its endpoints, which keeps the A* heuristic admissible. The track file format is line-based text (`VERTEX`, `EDGE`, `MAINROUTE`, `DETOUR`, `SHORTCUT`, `EVENT`, `LAPS`, `NAME`). It is parsed by `io/TrackLoader` and written by `io/TrackWriter`, so generated tracks round-trip.

## 4.2 Route cost and normalisation

Every router (Dijkstra, A*, the checkpoint planner, opponents, the resource planner) minimises, and every screen displays, **the same function** `model::EdgeCost`:

cost(e) = w_d · D(e)/D_ref + w_t · T(e)/T_ref + w_f · F(e)/F_ref + w_r · (effDifficulty(e) − 1)/4 + w_o · O(e)

| Term | Raw quantity | Normalisation |
|---|---|---|
| D | road length | ÷ mean base length of the circuit's roads |
| T | effective travel time = distance / effective speed (speed falls 8% per effective-difficulty level above base, and is capped by an active speed restriction) | ÷ mean base travel time |
| F | fuel needed = fuelCost × (1 + 0.1 × (effDifficulty − 1)) | ÷ mean base fuel cost |
| R | difficulty penalty | (effDifficulty − 1)/4 ∈ [0, 1] |
| O | obstacle penalty (0.5 while an oil spill is active) | already in [0, 1] |

The references are **means of the base values**, computed once when the circuit is loaded (`ComputeReferences`). They don't depend on the preset or on live effects. Dividing by a mean makes each of D, T and F about 1 for a typical road, so the weights compare like with like. R and O are already in [0, 1].

| Preset | w_d | w_t | w_f | w_r | w_o | Rationale |
|---|---|---|---|---|---|---|
| Balanced | 0.25 | 0.25 | 0.20 | 0.15 | 0.15 | moderate weight on every criterion |
| Speed | 0.15 | **0.55** | 0.05 | 0.15 | 0.10 | time dominates, fuel nearly ignored |
| Economy | 0.20 | 0.10 | **0.55** | 0.05 | 0.10 | fuel dominates |
| Safety | 0.10 | 0.15 | 0.10 | **0.35** | **0.30** | difficulty and obstacles dominate |
| Distance (internal) | 1 | 0 | 0 | 0 | 0 | Opponent 3's metric and the A* control in Experiment 2 |

All weights and all terms are ≥ 0, so cost(e) ≥ 0 and Dijkstra applies. **Presets choose different routes:** on C1 Harbor and C3 Downtown the Safety preset picks a longer lap (1 843 vs 1 720 units on C1, 2 080 vs 1 928 on C3) that avoids the difficult roads the other presets accept.

**Limitation of a single weighted scalar.** Folding five objectives into one number hides trade-offs: a route that is slightly worse on every criterion can never win, but neither can a route that is much better on one criterion and slightly worse on another unless the weights happen to favour it. The weights are a modelling choice, not a fact, and a different normalisation (for example dividing by the maximum instead of the mean) would change which route wins at the margin. Section 9 (Challenge 2) returns to this.

## 4.3 Game rules (frozen before testing)

All constants live in `src/model/Rules.h` (mirrored in `data/rules.cfg`). They were frozen before Phase 5 testing.

| Rule | Value |
|---|---|
| Simulation step | 20 ms, integer clock |
| Laps | 3 (setting: 2 or 3) |
| Fuel | tank 100.0 units, stored in integer tenths (0–1000); per edge fuelCost × (1 + 0.1 × (effDifficulty − 1)), charged gradually along the edge |
| Fuel station | up to 40.0 units per stop, fixed 3.0 s stop for any positive purchase, one purchase per visit |
| Nitro | start with 1 charge (setting 0–3), max 3; 1.5× speed for 4 s, 2× fuel while active; collected from active pickups (a pickup is used up when collected) |
| Oil spill | +2 effective difficulty (cap 5), obstacle penalty 0.5, 20 s |
| Speed restriction | speed cap = 50% of base speed, 15 s |
| Station outage | 30 s |
| Nitro booster | pickup active for 25 s or until collected |
| Recovery policy | the same for every racer: a racer with no valid route (or not enough fuel for its next road and no reachable station) **waits** at its vertex and re-plans after every graph change; if it is still stuck after **60 s** it is **DNF**. Waiting time counts toward its race time. |
| Race end | 60 s after the leader finishes, hard cap 15 min |
| Finish order | finished racers by finish time, ties by lower racer id; then unfinished racers by laps completed, checkpoints completed in the current lap, elapsed time, racer id |
| Points | 25-18-15-12-10-8-6-4-2-1 for **finished** racers; +1 for the fastest lap (among finishers). Unfinished racers are classified but score 0. |
| Championship order | points ↓, wins ↓, sum of finish positions ↑, best fastest lap ↑, racer id ↑ (one comparator for every sort and top-k) |

**Discrete units.** Fuel is kept in tenths of a unit so the refuelling DP has integer states. This keeps 0.1-unit precision but multiplies the fuel dimension by 10 (C = 1 000 states instead of 100), which is why the DP's table has 1 001 columns. Detour costs for the knapsack are in tenths of a second for the same reason.

**Determinism.** There is no `rand()`, no wall-clock input and no unordered iteration. All randomness (generator, benchmark inputs) comes from a seeded xorshift generator, and every tie is broken explicitly by id. The race engine exposes an FNV-1a hash of the results, and a test runs the same race twice and compares hashes.

## 4.4 Deterministic opponents (section 4.4 of the brief)

| | Route | Blocked road | Refuelling |
|---|---|---|---|
| Opponent 1 (fixed route) | the circuit's stored `MAINROUTE` loop | splices the documented `DETOUR` for that edge; if none, waits (recovery policy) | first eligible station on its own route when below 25% or short for the rest of its route |
| Opponent 2 (shortcut lover) | `MAINROUTE`, substituting a preferred shortcut whenever it is open and its start lies on the remaining route (never one that would skip a checkpoint) | falls back to the main route when the shortcut closes | as Opponent 1 |
| Opponent 3 (shortest distance) | Dijkstra under the Distance weighting, checkpoint by checkpoint | re-plans from scratch | nearest reachable station |
| Human (autopilot) | Dijkstra/A* under the chosen preset, checkpoint by checkpoint | re-plans | refuelling-DP recommendation; nearest station if short |

Every strategy is a pure function of (graph, racer, context): same track, same events, same starting state, same behaviour.

## 4.5 Dynamic events and overlapping effects

Events sit in a binary min-heap keyed by the integer triple (triggerTime, priority, insertionSequence). Priorities follow the brief: block/clear 1, shortcut 2, oil 3, speed restriction 4, station 5, nitro booster 6. All events due at the current time are applied in heap order first, and routes are recalculated once afterwards.

Each timed effect is stored as its **own node** in the affected edge's effect list, with its own effect id, and schedules its **own expiry event**. After any change the edge's effective state is recomputed from all effects still active, in this documented order:

1. `blocked` = explicitly blocked (road block, or a closed shortcut);
2. `effDifficulty` = min(5, base difficulty + the **largest** active oil bonus);
3. `speedCap` = base speed × the **smallest** active cap fraction (the strictest restriction);
4. `status` = Blocked, else Restricted if any oil or cap is active, else Open.

An expiry removes only its own effect id and recomputes, so two overlapping oil spills on one road behave correctly: when the first expires the road stays oily until the second does (tested in `EffectManager_OverlappingOilSpills_ExpiringOneKeepsTheOtherActive`).

**Validation of targets.** When an event fires, the race engine checks its target against the current graph: the edge or vertex must exist, and must be of the right kind for the event type (a shortcut for a shortcut toggle, a fuel station for an outage, a nitro pickup for a booster). An invalid event is rejected with a log line ("FuelStationToggle rejected: vertex 7 is not a fuel station") instead of touching the wrong element. An unknown event type in a track file is ignored with a log line when the race starts.

**Cancellation: lazy deletion.** `Cancel(id)` only marks the event, and `ProcessUntil` discards cancelled entries when they reach the top. This makes cancellation O(1) and the heap needs no decrease-key or position index. The cost is memory: a cancelled entry stays in the heap (and its pool slot stays allocated) until its trigger time comes up. A race schedules a few dozen events at most, so this costs a few hundred bytes. In the game, cancellation happens when a racer collects a nitro booster: the booster's pending expiry is cancelled.
