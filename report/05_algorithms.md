# 5. Algorithm Design

*Backend section — Muhammad Wasay Tariq (24I-3071)*

Every algorithm is hand-written in C++17 without the STL, `<algorithm>` or smart pointers (`ProjectRestrictions.txt`). Each one takes an optional `AlgoStats*` (operation counters for the experiments and the Lab) and an optional `Trace*` (step recorder for the Algorithm Lab). Gameplay passes `nullptr` for both, so the Lab and the benchmarks run **the same function** the game runs.

## 5.0 Supporting data structures (not algorithms)

The brief asks the report to separate algorithms from the data structures they use. These are the structures; sections 5.1–5.12 are the algorithms.

| Structure | File | Used by | Operations |
|---|---|---|---|
| `DynArray<T>` (growable array, doubling) | `core/ds/DynArray.h` | everywhere | push amortised O(1), index O(1) |
| `Queue<T>` (circular buffer) | `core/ds/Queue.h` | BFS | push/pop O(1) |
| `Stack<T>` | `core/ds/Stack.h` | iterative DFS | push/pop O(1) |
| `LinkedList<T>` | `core/ds/LinkedList.h` | adjacency lists, per-edge effect lists | push O(1), remove-if O(len) |
| `MinHeap<T, Policy>` (binary heap, array-backed) | `core/ds/MinHeap.h` | Dijkstra, A*, event queue, top-k | push/pop O(log n), top O(1), heapify O(n) |
| `UnionFind` (union by rank, path compression) | `core/ds/UnionFind.h` | Kruskal, Experiment 1 checks | near-O(1) amortised (inverse Ackermann) |
| `Pool<T>` (block allocator) | `core/ds/Pool.h` | event objects | allocate O(1) |
| `Graph` (adjacency lists) | `model/Graph.h` | all graph algorithms | neighbours O(deg v), status change O(1) |

There is one binary-heap implementation. Dijkstra, A*, the event queue and top-k all reuse it through a small ordering "policy" struct, so the heap code itself is tested once.

## 5.1 Breadth-first search (`algo/Bfs.cpp`)

**Role.** Reachability and the path with the fewest roads (Explorer "Reachability" tab, validator leg checks, generator connectivity).

```
BFS(G, s, t):
    dist[v] = -1 for all v; parent[v] = -1; dist[s] = 0; Q.push(s)
    while Q not empty:
        u = Q.pop()
        for each road (u, v) that is not blocked:
            if dist[v] == -1:
                dist[v] = dist[u] + 1; parent[v] = u; Q.push(v)
                if v == t: stop        // optional early exit
    path = follow parent[] back from t
```

**Correctness.** Vertices leave the FIFO queue in non-decreasing order of `dist` (layer by layer): every vertex at distance d is enqueued before any at distance d + 1. A vertex is labelled the first time it is reached, from a vertex of the smallest possible layer, so `dist[v]` is the minimum number of roads from s. BFS minimises **road count, not distance**. The brief's example (S–T of length 10 vs S–A–B–T of three roads of length 2) returns the 1-road path (test `Bfs_MinimumEdgeCount_NotMinimumDistance`, and an Experiment 1 check).

**Complexity.** Each vertex is enqueued at most once and each adjacency entry is scanned at most once: O(V + E) time, O(V) extra space (dist, parent, queue). An unreachable target ends when the queue empties, with no infinite loop.

## 5.2 Depth-first search (`algo/Dfs.cpp`)

**Role.** Traversal, cycle detection, connected components, bridges, dead ends and bounded alternative routes (Explorer "Structure" tab, validator, generator).

```
DFS-Traversal(G, s):                       // iterative: explicit stack of (vertex, next-neighbour cursor)
    colour[v] = WHITE for all v; time = 0
    push s; colour[s] = GRAY; disc[s] = time++
    while stack not empty:
        (u, cursor) = top
        if cursor has another unblocked road (u, v):
            advance cursor
            if colour[v] == WHITE: colour[v] = GRAY; disc[v] = time++; parent[v] = u; push v
        else:
            colour[u] = BLACK; finish[u] = time++; pop
```

- **Cycle detection:** an edge to a GRAY vertex (on the current DFS path) closes a cycle, unless it is the same undirected edge we arrived by. This works for a mix of directed and undirected roads. The witness cycle is read off the parent chain.
- **Components:** restart DFS from every still-white vertex (roads treated as undirected); each restart is a new component.
- **Bridges (Tarjan):** low[u] = min(disc[u], disc of back-edge targets, low of children); tree edge (u, v) is a bridge iff low[v] > disc[u].
- **Dead ends** (brief §5.1: a non-mandatory vertex from which the finish cannot be reached without reversing): (a) every vertex that cannot reach the finish in the directed graph (reverse search from the finish), plus (b) for each bridge whose far side holds no Start, Finish or checkpoint, every vertex on that side (a spur the car must U-turn out of).
- **Alternative routes:** a depth-first enumeration of simple paths that differ from the main route by at least one road, **capped at 8 routes and depth 2V**. Enumerating all simple paths is exponential, so the cap is mandatory, and the result reports `truncated` when it is hit.

**Correctness.** The DFS parenthesis theorem: v is a descendant of u iff disc[u] < disc[v] < finish[v] < finish[u]. An edge into a GRAY vertex therefore points to an ancestor, which, together with the tree path, forms a cycle. Conversely, every cycle contains a back edge to whichever of its vertices DFS discovers first. Tarjan's bridge rule follows from the same theorem: (u, v) is a bridge exactly when no back edge from v's subtree reaches u or above.

**Complexity.** Traversal, cycles, components and bridges: O(V + E) time, O(V) space. The traversal is **iterative**, so the 100 000-vertex graphs of Experiment 1 can't overflow the call stack. The alternative-route search is exponential in the worst case but bounded by the caps above.

## 5.3 Dijkstra (`algo/Dijkstra.cpp`)

**Role.** Optimal routes under any preset (human autopilot, Opponent 3, route comparison, refuelling detours, knapsack detour costs, Explorer "Routes" tab).

```
Dijkstra(G, s, t, w):
    d[v] = +inf; d[s] = 0; H.push((0, s))
    while H not empty:
        (k, u) = H.pop()
        if u closed: continue          // stale heap entry (lazy decrease-key)
        close u; if u == t: stop
        for each unblocked road e = (u, v):
            if d[u] + cost(e) < d[v]:
                d[v] = d[u] + cost(e); parent[v] = (u, e); H.push((d[v], v))
```

**Correctness.** All costs are ≥ 0 (section 4.2). Invariant: when u is popped for the first time, d[u] is the true shortest distance. Suppose not, and take a shorter path P to u. Let (x, y) be the first edge of P leaving the closed set. x was closed with a correct distance and relaxed (x, y), so d[y] ≤ length of P up to y ≤ length(P) < d[u]. Then y would have been popped before u, a contradiction. Ties in the heap are broken by vertex id, so equal-cost paths come out the same every run.

**Complexity.** With a binary heap and lazy deletion (pushing a new entry instead of decrease-key): O((V + E) log V) time, O(V + E) heap entries in the worst case. On our circuits a full lap (11 legs) takes microseconds.

## 5.4 A* (`algo/AStar.cpp`)

**Role.** Optional router for the human (Explorer and Lab comparison); returns the same optimal cost as Dijkstra.

Same loop as Dijkstra, with heap key f(v) = g(v) + h(v) and **h(v) = λ · euclid(v, t)**, where λ is the minimum cost-per-straight-line-unit over all roads:

λ = min over edges e = (u, v) of cost(e) / euclid(u, v)   (computed in one O(E) pass, `ComputeHeuristicScale`; λ = 0 falls back to h = 0, which is plain Dijkstra).

**Admissibility and consistency (proof).** By construction, every road satisfies cost(u, v) ≥ λ · euclid(u, v). By the triangle inequality, euclid(u, t) ≤ euclid(u, v) + euclid(v, t). Multiplying by λ ≥ 0:

h(u) = λ · euclid(u, t) ≤ λ · euclid(u, v) + λ · euclid(v, t) ≤ cost(u, v) + h(v).

So h is **consistent**. Since h(t) = 0, summing the inequality along any path from u to t gives h(u) ≤ cost of that path, so h is also **admissible**. With a consistent heuristic, the first time a vertex is popped its g is optimal (the f-values popped are non-decreasing), so **our A* never reopens a closed vertex**, and it returns the same optimal cost as Dijkstra. Live effects only raise costs or remove roads, so the bound still holds mid-race: λ is a lower bound for the base graph and every effect is non-negative.

This needs every road's length to be at least the straight-line distance between its endpoints. The validator enforces this on every track (check "Distances ≥ straight line"), and the hand-made circuits use exactly the straight-line length.

**Complexity.** Same worst case as Dijkstra, O((V + E) log V), plus the O(E) pass for λ (once per graph and preset). How much A* saves depends on how tight h is. Experiment 2 shows both cases: with the game presets λ is small (cost is dominated by difficulty and fuel terms, which have nothing to do with straight-line distance), so A* expands only 2–7% fewer vertices on long queries. Under the pure-distance weighting it expands 28% fewer, and on short queries 4–7× fewer.

## 5.5 Checkpoint-aware route planner (`algo/RoutePlanner.cpp`)

Not one of the 12 families, but the reason Dijkstra/A* are correct for racing. A lap is planned **leg by leg**: current vertex → next required checkpoint → … → CP10 → Finish, each leg a full Dijkstra or A* search. The concatenation can't skip a checkpoint, because each checkpoint is a leg's target. Already-completed checkpoints are skipped. If any leg is unreachable, the planner reports which leg failed and the race engine applies the recovery policy. Re-planning happens after every graph-changing event batch and at every lap boundary.

## 5.6 Dynamic programming

### 5.6a 0/1 knapsack for optional pickups (`algo/Knapsack.cpp`, `sim/ResourcePlanner.cpp`)

**Model.** The items are the active nitro pickups and fuel stations **not already on the racer's route**.
- **Cost c_i** = the cheapest insertion detour over the route: min over route roads (u, v) of time(u→p) + time(p→v) − time(u→v), using Dijkstra under travel time, plus the 3 s stop for a station. It is in tenths of a second.
- **Benefit v_i** (also tenths of a second): a nitro charge saves 2.0 s (4 s at 1.5× covers 6 s of road). A station is worth 60 s (the recovery wait it avoids), and is offered **only** when the race's refuelling plan is infeasible without a detour.
- **Budget B** = the allowed extra detour time (default 15.0 s = 150).

Cost, benefit and budget are all in one unit (seconds of race time), so the single budget is a defensible scalar: it doesn't mix fuel with time. Feasibility rules keep recommendations playable:

- at most (3 − current charges) nitro pickups are offered (a charge beyond capacity is worthless), and stations are offered only when fuel is actually needed;
- a detour is offered only where it can be **driven** from the racer's actual state: simulating the tank from the current fuel (topping up at active stations the route passes), the detour must never run dry, and the detoured route must complete the same checkpoints in the same order. Among those insertion points the cheapest in time is used;
- the knapsack treats items as independent, but two detours that each fit the tank may not fit it together. So the **chosen set** is re-checked as one route; while it is not driveable, the chosen item adding the most fuel is withdrawn and the DP (and greedy) are solved again on the smaller item set. DP and greedy are always compared on the same final items (tests `F04_…`).

**State and recurrence.** dp[i][b] = the best total benefit using items 1..i with total cost ≤ b.

- Base: dp[0][b] = 0 for every b (and dp[i][0] = 0 when all costs are positive).
- Transition: dp[i][b] = dp[i−1][b] if c_i > b, else max(dp[i−1][b], dp[i−1][b − c_i] + v_i).
- Reconstruction: walk i = m..1 from b = B; item i is taken iff dp[i][b] ≠ dp[i−1][b], then b −= c_i.

**Correctness.** Optimal substructure: in an optimal selection for (i, b), item i is either absent (the rest is optimal for (i−1, b)) or present (the rest is optimal for (i−1, b − c_i)). The recurrence takes the better of exactly these two cases, and the base case is trivially right. We also verify it against exhaustive search on 500 random instances, in the tests and again in Experiment 3.

**Complexity.** O(mB) time, O(mB) space for the full table (needed for reconstruction). `KnapsackValueOnly` keeps one row: O(B) space, value only. This is **pseudo-polynomial**: polynomial in the numeric value B, exponential in the number of bits needed to write B.

### 5.6b Refuelling plan (`algo/RefuelDp.cpp`)

**Model.** A fixed route (the one the planner chose) is cut into stops: stop 0 = where the racer is, stops 1..m−1 = the fuel stations the route passes (a station passed on two laps counts as two stops), stop m = the finish. leg_j = fuel (tenths) to drive from stop j to j + 1, C = 1 000 (tank), A = 400 (max purchase), stopTime(a) = 0 if a = 0, else 3 000 ms.

- State: dp[j][f] = the least total stop time on **arrival** at stop j holding f tenths.
- Base: dp[0][startFuel] = 0, every other state unreachable (+∞). The racer's actual fuel is used, so the plan also works mid-race.
- Transition: from a reachable (j, f), for every a ∈ [0, min(C − f, A)] (only a = 0 if stop j is not an active station): if f + a ≥ leg_j, then dp[j+1][f + a − leg_j] = min(itself, dp[j][f] + stopTime(a)).
- Answer: feasible iff some dp[m][f] is finite. Among the minimum-stop-time plans, the one arriving with the most fuel is returned (deterministic, and it leaves margin for later laps). Purchases are reconstructed from stored choices.

**Correctness.** Every real plan corresponds to exactly one path of transitions, and its stop time is the sum of the transition costs. The DP computes the minimum over all such paths stop by stop (shortest path in a layered DAG). Unreachable states are never expanded.

**Complexity.** O(m · C · A) = O(mC²) in this straightforward form. With tenths, C = 1 000 and A = 400, so a 10-stop race is about 4 million transitions, well under a millisecond in Release. Using whole units would cut the table 10× but round leg fuel to whole units, which would make some plans the race engine can actually execute look infeasible (or the reverse).

**Gameplay use.** The human's refuel pop-up is pre-filled with this plan's purchase for the current station (`ApiRecommendedRefuel`). The autopilot buys that amount; if the plan recommends nothing here but the car is short of fuel for its route, it buys the 40-unit maximum. The validator's "feasible fuel plan" check runs the same DP.

## 5.7 Greedy pickup heuristic (`algo/Knapsack.cpp`)

```
Greedy(items, B):
    take every item with cost 0 and benefit > 0          // free
    drop every item with benefit 0                        // useless
    sort the rest by benefit/cost descending, ties by lower id
    for each item in that order: if cost <= remaining: take it
```

Ratios are compared by **cross-multiplication** (v_a · c_b vs v_b · c_a, in 64-bit integers), so equal ratios really compare equal and the result never depends on floating-point rounding. The sort is our Merge Sort. Time O(m log m), space O(m).

The result is feasible, deterministic and **not optimal in general**. The brief's example: A(6, 30), B(5, 24), C(5, 24), budget 10. Greedy takes A (best ratio 5.0) and nothing else fits, for 30. The DP takes B + C for 48. Our two team-made counterexamples:
- **G1**: costs {4, 3, 3}, benefits {9, 6, 6}, B = 6. Greedy takes the 4-cost item (ratio 2.25) for 9; DP takes both 3-cost items for 12.
- **G2**: costs {5, 4, 4, 2}, benefits {11, 8, 8, 1}, B = 8. Greedy takes 11, then only the 2-cost item fits, for 12; DP takes 4 + 4 for 16.

## 5.8 Backtracking: constraint-based track generation (`sim/TrackGenerator.cpp`)

**Role.** Challenge 1 and the Lab's generator module. Generated tracks are saved and become playable through `ApiLoadCircuitFile`.

Pipeline: jittered grid of vertices → candidate roads (each vertex to its 4 nearest) → **connectivity check** (a disconnected candidate graph would give a spanning forest, so the attempt is rejected) → **Kruskal MST** backbone (5.9) → extra roads for loops (shortest remaining candidates, degree ≤ 4), the two longest of which become difficulty-5 shortcuts → **backtracking placement** → **backtracking obstacle placement** → fuel scaling → **validator**. If the validator rejects the track, the next attempt runs with a seed derived from the original, up to 50 attempts.

```
Place(slot):                                   // slots: Start, CP1..CP10, Finish, Fuel1, Fuel2, Nitro1, Nitro2
    if slot == end: return true
    for v in candidate order:
        if not Admissible(slot, v): pruned++; continue
        assign v to slot; nodes++
        if Place(slot + 1): return true
        unassign v                              // backtrack
    return false
```

**Pruning rules** (each cuts a branch before recursing; why each is safe):

| Rule | What it prunes | Why it is safe |
|---|---|---|
| Leaves (degree-1 vertices) never get a role | every branch that puts a role on a spur | keeps at least one dead end, which the brief requires; a checkpoint on a spur would also force a U-turn every lap |
| Consecutive checkpoints are 2–3 roads apart | checkpoints adjacent to, or far from, the previous one | only discards layouts that are trivial (adjacent) or very long; any layout it keeps is still playable |
| Finish adjacent to Start and within 3 roads of CP10 | Finish positions that don't close the lap | a lap must return from CP10 to the Finish and on to the Start; layouts violating this would fail the validator's "lap returns" check anyway |
| The two fuel stations ≥ 3 roads apart | clustered stations | clustered stations serve one part of the lap; this only removes layouts with worse fuel coverage |
| Obstacles only on non-bridge roads, not touching Start or Finish, not sharing a vertex | blocks that would disconnect the track, block the start line or stack on one junction | closing a bridge cuts the graph in two, so no route would exist while it is closed; the validator's connectivity check would reject it anyway |
| Node limit (200 000 assignments) | runaway searches | turns a pathological attempt into a clean rejection with a reason, and the next attempt starts |

**Complexity.** Exponential in the worst case: with k slots and n candidate vertices, the search tree has up to n^k leaves. Pruning makes the common case fast. With seed 1 the accepted attempt explored 1 880 assignments and pruned 50 468 candidates, while a pathological attempt (seed 34, attempt 0) hit the 200 000 node limit and was rejected. Several attempts per seed are normal. In the generator tests, 10 or more of seeds 1–12 produce valid tracks, and every accepted track passes the validator.

**Rejected candidate example** (the brief asks for one). Seed 2, attempt 0: the placement succeeded, but the validator rejected the track: "Feasible fuel plan: no refuelling plan finishes 3 laps, even via a fuel station each lap". Attempt 1 was accepted. Seed 3 is rejected nine times ("only one route connects Start and Finish") before attempt 9 passes.

## 5.9 Kruskal's minimum spanning tree (`algo/Kruskal.cpp`)

```
Kruskal(V, candidates):
    sort candidates by (weight, id) with Merge Sort
    UF.init(V)
    for each road (u, v) in that order:
        if UF.find(u) != UF.find(v): accept it; UF.union(u, v)
        else: reject it (it would close a cycle)
```

**Correctness (cut property).** When a road is accepted it is the lightest road crossing the cut between its two endpoints' current components (every lighter road was examined earlier and either joined components or lay inside one). By the cut property, the lightest road across any cut belongs to some MST. So every accepted road is safe, and the result is an MST of each component, a spanning **forest** if the candidate graph is disconnected. That's why the generator checks connectivity first. Ties are broken by road id, so the tree is deterministic.

**Complexity.** O(E log E) for the sort plus O(E α(V)) for union-find: O(E log E) overall, O(V + E) space.

**Role in the game.** The MST is the backbone of every generated track. On its own a tree has no cycles and no alternative routes, so the extra roads of step 5 are always added afterwards (the brief's warning that "an MST alone has no cycles"). The Lab also runs Kruskal on the loaded circuit's roads.

## 5.10 Track validator (`sim/TrackValidator.cpp`)

Not one of the 12 families, but it ties several together. Every loaded or generated track is checked against **11 named checks**: Start and Finish present; checkpoint orders exactly 1..K, each once, with K ≥ 10; every road's length ≥ its straight-line distance (A* admissibility); connected; checkpoints reachable in order (BFS per leg, directed); the lap returns from the Finish to CP1; has a cycle (DFS); has a dead end (DFS); at least 2 shortcuts; an alternative Start–Finish route (bounded DFS); a feasible fuel plan (refuelling DP over a full race, first on the plain route, then via each active station every lap). A failure carries a reason that the Explorer and the generator report display.

## 5.11 Binary heap priority queue

**Event queue** (`sim/EventQueue.cpp`): min-heap of `Event*` keyed by (triggerTime, priority, insertionSequence), all integers, so ordering never depends on floating-point equality, and the sequence number makes every tie deterministic. Cancellation is lazy deletion (section 4.5). Events are allocated from a pool, and an id → event array gives O(1) cancel and lookup.

```
Schedule(type, time, target, duration): e = pool.new(...); e.seq = nextSeq++; H.push(e)      // O(log n)
ProcessUntil(now): while H.top.time <= now: e = H.pop(); if not e.cancelled: apply(e)         // O(log n) per pop
```

**Top-k** (`algo/TopK.h`, Challenge 3), two methods, both using the championship comparator:
- **Size-k heap:** keep the best k seen so far in a heap whose top is the *worst* of them; a new entry enters only if it beats the top. O(n log k) time, O(k) space.
- **Heapify + k extractions:** build a heap of all n in O(n), pop k times. O(n + k log n) time, O(n) space.

Both return exactly the first k entries of a full sort (tested, and checked in Experiment 5 for k = 1, 10, n/2, n − 1).

## 5.12 Sorting: Merge, Quick and Heap Sort (`algo/Sorting.h`)

All three take the same comparator policy. For standings, that is `StandingOrder`: points ↓, wins ↓, sum of finish positions ↑, best lap ↑, racer id ↑. This is a **total order** (racer ids are unique), so all three must produce the identical ranking, and the dashboard checks that they do.

**Merge Sort** (primary ranking; also the finish order, the greedy ratio order and Kruskal's edge order):
```
MergeSort(A, lo, hi): if hi - lo < 2: return
    mid = (lo + hi) / 2; MergeSort(A, lo, mid); MergeSort(A, mid, hi)
    merge the two runs into a buffer, taking from the LEFT run on ties; copy back
```
Stable (left-first on ties). Θ(n log n) comparisons in every case, O(n) extra space. Correctness: induction on the run length, since merging two sorted runs yields a sorted run.

**Quick Sort** (alternative ranking):
```
QuickSort(A, lo, hi):
    while lo < hi:
        pivot = median of A[r1], A[r2], A[r3] for 3 positions from a seeded generator; move it to A[lo]
        3-way partition: [lo, lt) < pivot, [lt, gt] == pivot, (gt, hi] > pivot
        recurse into the smaller side, loop on the larger side
```
In place, not stable. O(n log n) expected for every input order, O(log n) stack (always recursing on the smaller side). O(n²) worst case, but only for an input built against the fixed seed. Equal keys are grouped by the 3-way partition and never recursed into again.

*Pivot choice and a measured fix.* The first version took the median of the elements at fixed positions lo, mid and hi. Experiment 5 showed it degrading on sorted and reverse-sorted input: 22 million comparisons for 100 000 sorted records against 1.9 million for random ones, and the ratio grew with n. The cause is an interaction with the 3-way partition: it swaps each "greater" element to the end of the range, which reverses that block, and the sub-arrays it then recurses into take on patterns that defeat a fixed-position median. Sampling the three positions from a seeded generator removes the dependence on input order but stays deterministic (same input → same steps → same Lab trace). Every input order now costs 1.0–1.14 · n log₂ n comparisons at every size we tested.

**Heap Sort** (third method): bottom-up heapify in O(n), then n − 1 times swap the root to the end and sift down. Θ(n log n) in every case, in place, not stable. It does the most comparisons of the three (about 2 n log₂ n, against Merge Sort's ~n log₂ n), which Experiment 5 confirms.

## 5.13 Summary of complexities

| # | Algorithm | Time | Extra space | Notes |
|---|---|---|---|---|
| 1 | BFS | O(V + E) | O(V) | fewest roads |
| 2 | DFS (traversal, cycles, components, bridges) | O(V + E) | O(V) | iterative; alternative routes bounded (8 routes, depth 2V) |
| 3 | Dijkstra | O((V + E) log V) | O(V + E) | binary heap, lazy decrease-key |
| 4 | A* | O((V + E) log V) worst | O(V + E) | consistent h, no reopening |
| 5a | Knapsack DP | O(mB) | O(mB) (O(B) value-only) | pseudo-polynomial |
| 5b | Refuelling DP | O(m C A) = O(m C²) | O(m C) | tenths: C = 1 000 |
| 6 | Greedy pickups | O(m log m) | O(m) | not optimal |
| 7 | Backtracking placement | exponential worst case | O(V) | pruning + node limit |
| 8 | Kruskal | O(E log E) | O(V + E) | union-find |
| 9 | Heap PQ: event queue / top-k | O(log n) per op / O(n log k) or O(n + k log n) | O(n) / O(k) | lazy deletion |
| 10 | Merge Sort | Θ(n log n) | O(n) | stable |
| 11 | Quick Sort | O(n log n) expected, O(n²) worst | O(log n) | seeded median-of-3, 3-way |
| 12 | Heap Sort | Θ(n log n) | O(1) | not stable |
