# 8. Experimental Studies

*Backend section — Muhammad Wasay Tariq (24I-3071)*

## 8.0 Common method

**Environment.** Intel Core i7-1265U (12th gen, 10 cores / 12 threads, base 1.8 GHz, turbo boost on), 31.4 GB RAM, Windows 11 Pro (build 26300), Balanced power plan on mains power. MSVC 19.51 (Visual Studio 2026), x64, **Release** build (/O2), CMake. Single-threaded. Timer: `QueryPerformanceCounter`, in microseconds (`core/platform/Timer_win.cpp`). The full description is in `results/hardware.txt` and `results/summary/environment.txt`.

**Procedure** (`bench/`, executable `smartrace_bench all`, about 3½ minutes):
- Inputs are generated from **fixed seeds** (default 42), so every run and every competing algorithm sees the same instance.
- Input generation and all correctness checks happen **outside** the timed region. Only the algorithm call is timed.
- Each configuration: **2 warm-up runs, then 7 timed runs**. We report the **median** (robust to the occasional slow run on a laptop CPU), plus the mean, standard deviation, minimum and maximum in the summary CSVs.
- Calls shorter than the timer's resolution are repeated `reps` times inside one timed region and divided by `reps` (the `reps` column).
- Timings **include** each algorithm's own working storage (result arrays, heap, DP table), stated per row in the `includes_init` column. Sort timings exclude copying the input (a fresh copy is made before the timer starts).
- No rendering, file I/O or delays happen inside timed regions. The CSVs are written afterwards.

**Reproducing.** `cmake --build build --config Release`, then from the repository root `build/Release/smartrace_bench.exe all` writes `results/raw/` (every sample) and `results/summary/` (statistics, counters, correctness). `python tools/plot_results.py` draws `results/charts/` and writes the full tables to `results/RESULTS.md`.

**Theory vs measurement.** For each experiment we state the theoretical bound first, then check it against the **operation counters**, which are exact and machine-independent. Wall-clock time is reported alongside but treated with care: on this hardware it is distorted by caches, turbo frequency and allocation, so we never infer a complexity class from timings alone.

---

## 8.1 Experiment 1 — BFS and DFS

**Objective.** Measure BFS and the DFS variants as the graph grows, on sparse and denser graphs. Verify BFS's minimum road counts and DFS's cycle and component results. Compare the growth with O(V + E).

**Input generation** (`bench/GraphGen.cpp`, `MakeRandomGraph`). Random connected undirected graphs: a random tree (each vertex joins a random earlier one), then random extra roads until the average degree is **3 (sparse)** or **12 (denser)**. Vertex coordinates are uniform in a square. **Sizes:** V ∈ {1 000, 5 000, 10 000, 50 000, 100 000}, so E from 1 500 to 600 000.

**Metrics.** Median time, vertices visited and edges examined, for BFS (full reachability from vertex 0), DFS traversal, DFS components and DFS cycle detection.

**Correctness (untimed, all passed).**
- BFS hop distances equal a brute-force fixed-point relaxation on every graph, and satisfy the BFS certificate (|d(u) − d(v)| ≤ 1 on every road; each reached vertex's parent is one hop closer).
- DFS reaches every vertex of each connected graph.
- DFS components match union-find on a graph built with 4 components.
- DFS finds a cycle in every test graph and none in a forest.
- The brief's S–T example returns the 1-road path.

**Results** (median µs; full table in `results/RESULTS.md`; charts `exp1_time_vs_size.png`, `exp1_normalised.png`):

| V | E (sparse) | BFS | DFS traversal | E (dense) | BFS | DFS traversal |
|---|---|---|---|---|---|---|
| 1 000 | 1 500 | 16 | 21 | 6 000 | 96 | 107 |
| 10 000 | 15 000 | 675 | 900 | 60 000 | 5 034 | 4 856 |
| 100 000 | 150 000 | 22 359 | 20 002 | 600 000 | 318 244 | 341 669 |

Counters: BFS and DFS traversal visit exactly V vertices and examine exactly 2E adjacency entries (each undirected road from both ends) at every size, for example 100 000 and 1 200 000 on the largest dense graph.

**Interpretation.**
- *Counters:* the work counted is exactly linear in V + E at every size, which is the O(V + E) bound made concrete: each vertex is enqueued or pushed once and each adjacency entry is scanned once.
- *Timings:* time per (V + E) element grows from about 7–15 ns at V = 1 000 to 80–490 ns at V = 100 000. The counters say the work per element is constant, so the growth is the **memory hierarchy**: a random graph's neighbours are scattered in memory, and once the adjacency lists, the visited array and the queue outgrow the CPU caches (around V = 10 000 on the dense graphs) each step costs a cache miss. Sparse graphs fit longer and degrade later.
- *Sparse vs dense:* at equal V, dense graphs take about 6× longer at V = 1 000 (4× more edges plus some overhead) and 14–17× longer at V = 100 000, because the dense graphs outgrow the caches sooner.
- *Cycle detection* stops at the first back edge, so it examines only a few hundred edges at any size (for example 550 at V = 100 000). Its cost depends on where the first cycle is found, not on the graph's size.

**Limitations.** Random graphs are the worst case for memory locality. Our race circuits (about 25 vertices) are entirely cache-resident, so these large-graph timings overstate per-element costs for the game. A laptop CPU with turbo boost adds run-to-run variance (see the standard deviation column).

**Conclusion.** BFS and DFS do linear work, confirmed exactly by the counters. Wall-clock growth beyond linear at large V is a cache effect, not an algorithmic one.

---

## 8.2 Experiment 2 — Dijkstra and A*

**Objective.** Compare Dijkstra and A* across circuits and graph sizes. Record time, expanded vertices, relaxed edges, heap operations and path cost. Verify equal optimal costs, and discuss when A* expands fewer, equal or more vertices.

**Inputs.**
- The **3 real circuits**, queries Start → CP5 and CP5 → CP10 (route legs the race planner really solves).
- **Road-network grids** (`MakeGeometricGrid`): V ≈ {1 024, 5 041, 10 000, 50 176, 99 856} vertices on a jittered square grid with 100-unit spacing, 4-neighbour roads plus 20% diagonals. distance = straight-line length × (1 + 0–25%), so distance ≥ straight line everywhere (the admissibility condition). Difficulty is uniform 1–5, speed 30 − 4·difficulty units/s, fuel = 0.02 × distance × (1 + 0.1(difficulty − 1)).
- Queries: **far** (top-left corner → bottom-right corner) and **near** (centre → a vertex about 10% of the grid away).
- All **4 presets**, plus the pure-**Distance** weighting as a control.

Cost references and A*'s heuristic scale λ are computed outside the timed region. λ's own O(E) cost is reported separately (row `AStar_scale`).

**Correctness (untimed, all passed).** For every graph, query and preset (80 combinations), A*'s cost equals Dijkstra's to a relative error below 10⁻⁹, and both find a path.

**Results** (largest grid, V = 99 856; charts `exp2_grid_scaling.png`, `exp2_expanded_by_preset.png`):

| Query | Preset | Dijkstra expanded | A* expanded | Dijkstra µs | A* µs | heuristic scale λ |
|---|---|---|---|---|---|---|
| far | Balanced | 99 856 | 93 995 (−6%) | 78 526 | 91 865 | 0.0048 |
| far | Safety | 99 856 | 98 199 (−2%) | 77 782 | 90 744 | 0.0023 |
| far | Distance | 99 856 | 71 522 (−28%) | 61 208 | 62 908 | 0.0083 |
| near | Balanced | 6 335 | 1 524 (4.2× fewer) | 5 693 | 2 008 | 0.0048 |
| near | Distance | 6 442 | 957 (6.7× fewer) | 6 112 | 1 958 | 0.0083 |

On the real circuits (about 25 vertices), A* expanded 4–12 vertices where Dijkstra expanded 8–21, and both finished in a few microseconds (at most 5.6 µs).

**Interpretation.**
- *Optimality* holds everywhere, as the consistency proof (section 5.4) predicts.
- *When A* helps.* A* expands fewer vertices than Dijkstra in every test here, but by very different margins:
  - **Short queries:** 4–7× fewer, and about 3× faster. The heuristic's guidance matters most when the target is close.
  - **Long queries under the game presets:** only 2–7% fewer. The heuristic is λ·euclid, where λ is the *minimum* cost per unit of straight-line distance over all roads. With weighted costs dominated by difficulty and fuel terms, the cheapest road per unit length is far cheaper than a typical road, so λ·euclid badly underestimates the remaining cost and A* behaves almost like Dijkstra. Safety, where difficulty weighs most, has the smallest λ and the smallest gain.
  - **Pure distance:** λ is tight and A* skips 28% of the graph.
- *Wall-clock.* When the expansion saving is small, A* is **slower** than Dijkstra (Balanced far: 92 ms vs 79 ms, Safety 91 vs 78), because each push also computes a square root for h. Under pure distance the 28% saving roughly pays for that overhead (63 vs 61 ms, within the run's spread of 11–17 ms). Dijkstra and A* are timed in alternating runs, so both see the same CPU conditions. A* can also expand *equal or more* vertices than Dijkstra: with h ≡ 0 (λ = 0, which happens when some road has zero cost) it expands exactly the same set, and tie-breaking on equal f-values can make it expand slightly more on graphs with many equal-cost paths. We didn't observe "more" in these inputs.
- *Heap operations* track expansions: under Dijkstra, about 2.5 per vertex on the grid (lazy decrease-key pushes duplicates).

**Limitations.** A tighter heuristic (for example a separate λ per cost term, or landmarks) would help the game presets but needs a separate admissibility proof. The grids are planar and regular; real road maps have more variation. Timings for sub-microsecond circuit queries are dominated by timer resolution, so the batched averages there are approximate.

**Conclusion.** A* always matched Dijkstra's optimal cost. Its benefit depends on the heuristic's tightness: large for short queries and pure-distance routing, small for long queries under weighted multi-objective costs. That's why the game keeps both: A* for the human autopilot, where legs are short, and Dijkstra elsewhere.

---

## 8.3 Experiment 3 — DP and Greedy

**Objective.** Vary item count and budget. Record benefit, selected items, runtime and DP state count. Include the supplied example and at least two cases where greedy is strictly worse. Compare the DP with exhaustive search on small instances.

**Input generation.** m ∈ {5, 10, 20, 50, 100, 200} items, budget B ∈ {50, 500, 5 000}. Costs are uniform in 1..4B/m (total cost ≈ 2B, so roughly half the items fit). Benefits are either **uncorrelated** (uniform over the same range) or **correlated** (cost ± 10%, the harder case for greedy). Plus the brief's A/B/C example and our cases G1 and G2. Algorithms: DP with full table (and reconstruction), value-only DP (one row), greedy, and exhaustive search (m ≤ 20).

**Correctness (untimed, all passed).** DP = exhaustive on every m ≤ 20 configuration, on 500 random small instances (m 1–12, B 0–40, zero costs and benefits allowed) and on the named cases. Value-only DP = DP on every configuration.

**Named cases** (`results/summary/exp3_cases.csv`):

| Case | Items (cost, benefit) | B | DP | Greedy | Exhaustive |
|---|---|---|---|---|---|
| Brief | A(6, 30), B(5, 24), C(5, 24) | 10 | **48** (B, C) | 30 (A) | 48 |
| G1 (team) | (4, 9), (3, 6), (3, 6) | 6 | **12** (items 1, 2) | 9 (item 0) | 12 |
| G2 (team) | (5, 11), (4, 8), (4, 8), (2, 1) | 8 | **16** (items 1, 2) | 12 (items 0, 3) | 16 |

**Timings** (uncorrelated, median µs; chart `exp3_time.png`):

| m | B | DP (table) | DP (value only) | Greedy | Exhaustive | DP states |
|---|---|---|---|---|---|---|
| 5 | 50 | 2.3 | 0.8 | 1.0 | 0.9 | 255 |
| 20 | 50 | 3.0 | 1.7 | 0.7 | 54 290 | 1 020 |
| 20 | 5 000 | 144 | 69 | 0.8 | 65 145 | 100 020 |
| 200 | 5 000 | 2 420 | 886 | 13.0 | — | 1 000 200 |

**Greedy quality** (50 fresh instances per configuration; chart `exp3_greedy_quality.png`): greedy was strictly worse than DP in up to 44 of 50 instances. Its mean ratio to the optimum was 0.90–0.92 for small correlated instances (m = 5), with a worst case of 0.42 (m = 5, B = 50). It approaches 1.00 as m grows (mean ≥ 0.996 at m = 50).

**Interpretation.**
- *DP scaling:* the states counter is exactly (m + 1)(B + 1), and once the table is large, time grows in proportion to m·B: about 1.4–2.4 µs per thousand cells, up to 1 million cells (small tables are dominated by fixed overhead). This is the pseudo-polynomial O(mB): B = 5 000 costs 100× more than B = 50 at the same m.
- *Value-only DP* uses one row (O(B) memory) and is 1.7–2.8× faster, because the smaller array stays in cache. But it can't say which items to take, and the game needs that.
- *Exhaustive search* does 2^m work independent of B: 54–65 ms at m = 20 for any budget, while DP needs 3–144 µs there. It is only usable as a correctness oracle.
- *Greedy* is O(m log m), microseconds even at m = 200. Its worst cases come with few items and correlated benefits: one large high-ratio item fills the budget and blocks a better combination of smaller ones, exactly the pattern of the brief's example and of G1/G2. With many small items, the leftover budget greedy wastes is a small fraction of the total, so it gets close to optimal.

**Limitations.** Random instances are not the game's instances. The game offers at most a handful of pickups, where DP's cost is negligible, which is why the game always uses DP and only shows greedy for comparison. Exhaustive timing was capped at m = 20 by design.

**Conclusion.** DP is always optimal (verified against exhaustive search) at pseudo-polynomial cost. Greedy is fast but can lose more than half the benefit on small adversarial instances.

---

## 8.4 Experiment 4 — Event priority queue

**Objective.** Vary the event count. Measure insertion and extraction time and heap operations. Test equal timestamps, priority and sequence ties, cancellations and expirations. Verify the order against (triggerTime, priority, insertionSequence).

**Inputs** (the race engine's own `sim::EventQueue`). n ∈ {1 000, 10 000, 100 000, 1 000 000} events, four workloads:
- **random:** times uniform over [0, 10n) ms, random event types;
- **tie_heavy:** only 10 distinct times, so most events tie on time and many on (time, priority), and the insertion sequence decides;
- **cancel_heavy:** random, then about half cancelled before processing;
- **with_expiry:** n/2 timed effects whose expiries are scheduled *while* popping, as in a race.

Phases are timed separately: insert (all `Schedule` calls), cancel, and extract (one `ProcessUntil` draining the heap). The counters are the heap's own comparisons and swaps.

**Correctness (untimed, all 16 configurations passed).** Delivered events are in non-decreasing (time, priority, sequence) order. For the three workloads without expiries, the delivered sequence **equals a Merge Sort** of all non-cancelled events by that key. Cancelled events are never delivered, and delivered counts are exact.

**Results** (median; chart `exp4_ns_per_op.png`, `exp4_heap_ops.png`):

| n | insert ns/op (random) | extract ns/op (random) | extract ns/op (tie-heavy) | comparisons + swaps per pop (random) | cancel ns/op |
|---|---|---|---|---|---|
| 1 000 | 71 | 189 | 317 | 22 | 2.3 |
| 10 000 | 107 | 458 | 579 | 32 | 6.3 |
| 100 000 | 127 | 1 075 | 1 099 | 42 | 11.4 |
| 1 000 000 | 150 | 3 194 | 1 016 | 52 | 8.3 |

**Interpretation.**
- *Extraction:* each pop does about 1.75 · log₂ n comparisons plus 0.9 · log₂ n swaps (sift-down), so (comparisons + swaps) per pop grows by a constant step each time n grows 10×: 22 → 32 → 42 → 52. That is O(log n), exactly.
- *Insertion* averages only about 2.2 comparisons and 1.2 swaps regardless of n. Inserting a random key into a binary heap rises O(1) levels on average (the O(log n) bound is the worst case, for a key smaller than everything already there).
- *Ties:* equal timestamps don't change the operation counts (the integer triple always has a strict order). Comparisons fall through to the second and third key more often, which made tie-heavy extraction slower up to n = 100 000 in this run; at n = 10⁶ it was faster, which is within the run-to-run variation discussed below.
- *Cancellation* is O(1) (a flag) at about 2–11 ns. Cancelled entries still cost a pop when they reach the top, so the cancel-heavy extract phase does the same number of pops as the random one but delivers half as many events. That is the memory and time cost of lazy deletion, paid for its simplicity.
- *Wall-clock* per extraction rises much faster than the logarithmic operation count, to 1–3 µs at n = 10⁶. At that size the heap's pointer array and the pooled events no longer fit in cache, and every comparison dereferences two event pointers. A race holds a few dozen events, where each operation costs tens of nanoseconds.

**Limitations.** The large runs are memory-bound and noisy on a laptop: at n = 10⁶, random insertion ran 2.7× slower than tie-heavy insertion (150 vs 56 ns) despite near-identical operation counts, and the whole suite ran 1.5–3× slower than an earlier run of the same binary. That points to clock-speed and allocation effects. Operation counts are the reliable comparison.

**Conclusion.** The heap does O(log n) work per extraction and O(1) average work per insertion, with an exactly deterministic order for every kind of tie. Lazy cancellation is O(1) at the cost of keeping dead entries until they surface.

---

## 8.5 Experiment 5 — Sorting and top-k

**Objective.** Compare Merge, Quick and Heap Sort on random, sorted, reverse-sorted and duplicate-heavy inputs. Vary n, and report runtime and comparison counts. Compare a full sort with size-k heap retrieval for small k and k close to n. Verify every ranking and top-k against the common comparator.

**Inputs.** `ChampionshipEntry` records ranked by the championship comparator. n ∈ {10, 100, 1 000, 10 000, 100 000}. Inputs:
- **random** (points 0–78, wins 0–3, position sum 3–30, best lap 50–90 s);
- **sorted** (already in comparator order);
- **reverse**;
- **duplicate** (only 24 distinct key combinations, so long runs differ only by racer id).

Racer ids are a random permutation in every input. For top-k: the random input at n ∈ {1 000, 10 000, 100 000}, k ∈ {1, 10, n/2, n − 1}, size-k heap vs heapify + k extractions vs a full Merge Sort.

**Correctness (untimed, all passed).** The three sorts produce identical rankings on every input and size, each strictly increasing under the comparator. The duplicate input really needs the racer-id tie-breaker: 76 of the 99 adjacent pairs at n = 100, and 99 976 of 99 999 at n = 100 000, are ordered only by racer id. Both top-k methods equal the first k entries of the full sort for every n and k.

**Sorting results** (n = 100 000; full table in `RESULTS.md`; charts `exp5_sort_time.png`, `exp5_comparisons.png`):

| Input | Merge µs | Quick µs | Heap µs | Merge comparisons | Quick comparisons | Heap comparisons |
|---|---|---|---|---|---|---|
| random | 56 090 | 64 234 | 81 460 | 1 536 402 | 1 910 262 | 3 019 449 |
| sorted | 22 259 | 45 595 | 64 837 | 815 024 | 1 975 411 | 3 112 517 |
| reverse | 22 691 | 50 121 | 66 949 | 853 904 | 2 008 684 | 2 926 640 |
| duplicate | 59 782 | 65 153 | 88 041 | 1 536 075 | 1 925 681 | 3 019 711 |

**Top-k results** (n = 100 000; chart `exp5_topk.png`):

| k | size-k heap: comparisons / µs | heapify + extract: comparisons / µs | full Merge Sort: comparisons / µs |
|---|---|---|---|
| 1 | 99 999 / 751 | 187 819 / 6 343 | 1 536 402 / 55 949 |
| 10 | 100 608 / 722 | 188 103 / 7 468 | (same) |
| 50 000 | 2 513 088 / 67 248 | 1 703 992 / 50 512 | (same) |
| 99 999 | 3 059 532 / 86 070 | 3 019 300 / 85 790 | (same) |

**Interpretation.**
- *Comparison counts match theory:* comparisons/(n log₂ n) stay flat as n grows, at about 0.9 (Merge), 1.0–1.15 (Quick) and 1.8 (Heap). That is the Θ(n log n) bound with each method's characteristic constant. Merge Sort does fewest because merging uses about one comparison per element per level. Heap Sort does about two per level of sift-down.
- *Input order:* Merge Sort halves its comparisons on sorted and reverse input (one run is exhausted early in each merge) and runs 2.5× faster. Heap Sort is insensitive to order, as expected. Quick Sort costs about the same on every order *after the pivot fix* (section 5.12). Before it, sorted input cost 22 M comparisons; the experiment caught a real defect.
- *Duplicates:* with this comparator no two records are truly equal (racer ids differ), so duplicate-heavy input behaves like random input for all three. The 3-way partition's advantage needs genuinely equal keys, as in the integer test `Sorting_Ints_AllMethodsSortDuplicates`.
- *Time vs comparisons:* Quick Sort does 24% more comparisons than Merge Sort on random input but is only about 15% slower, because it works in place while Merge Sort copies every element through a buffer at every level. Heap Sort's scattered access pattern makes it slowest at large n.
- *Top-k, effect of k relative to n* (Challenge 3):
  - For **small k**, the size-k heap is about **75× faster than sorting** (751 µs vs 56 ms at k = 1). It does about n comparisons, because nearly every element is rejected after one comparison with the heap's top.
  - Heapify + extract pays O(n) to build a heap of everything (≈ 1.9n comparisons) before extracting, so it is slower than the size-k heap for small k.
  - As **k approaches n** both lose their advantage. At k = n/2, heapify + extract is the better method, but both cost as much as a full sort. At k = n − 1 both do about 2 n log₂ n comparisons, *more* than Merge Sort's ~0.9 n log₂ n.
  - So: use a size-k heap for a small leaderboard and a full sort for complete standings, which is what the dashboard does.

**Limitations.** At n ≤ 100 timings are dominated by fixed overheads (batched averages of sub-microsecond calls), so only the counters are meaningful there. The championship itself has at most 10 racers, where every method takes well under a microsecond. The large-n results characterise the algorithms, not the game's load.

**Conclusion.** All three sorts and both top-k methods agree exactly under one comparator. Measured comparison counts follow Θ(n log n) with the expected constants, and a size-k heap beats full sorting by more than an order of magnitude when k ≪ n.
