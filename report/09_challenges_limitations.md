# 9. Advanced Challenges, Limitations and Future Work

*Backend section — Muhammad Wasay Tariq (24I-3071). Rayyan adds the GUI's limitations.*

## 9.1 Advanced challenges completed

### Challenge 1 — Automatic track generation: completed

The generator (`sim/TrackGenerator.cpp`, section 5.8) builds a circuit from an **MST backbone** (Kruskal over the 4-nearest-neighbour candidate roads, after checking they are connected), adds **extra roads** for loops and two shortcuts, places Start, ten ordered checkpoints, Finish, two fuel stations and two nitro pickups by **backtracking with documented pruning rules**, places obstacles only on non-bridge roads, and runs the full **validator**. Rejected attempts are retried with a derived seed, up to 50 times.

- **Invalid candidate rejected by the validator.** Seed 2, attempt 0 placed every role successfully, but the validator rejected the track: *"Feasible fuel plan: no refuelling plan finishes 3 laps, even via a fuel station each lap"*. The stations ended up too far from where the fuel runs out, so no sequence of purchases (≤ 40 units per stop, 100-unit tank) reaches the finish. Attempt 1 passed all 11 checks. Seed 3 shows the other common rejection: nine attempts fail with *"only one route connects Start and Finish"* (the extra roads did not create an alternative Start–Finish path), and attempt 9 is accepted.
- **Playable.** A generated track is saved in the same `.trk` format (`ApiLabSaveGenerated`), reloaded (`ApiLoadCircuitFile`) and raced like a hand-made circuit. A test generates, saves, reloads and re-validates one.
- **Visualised.** The Algorithm Lab's "Track Generator" module steps through the accepted attempt: Kruskal accepting and rejecting roads, extra roads and shortcuts marked, each placement and backtrack, and the obstacles. It also lists every rejected attempt with its reason.

### Challenge 2 — Multi-objective route planning: completed

Routes are compared under the Balanced, Speed, Economy and Safety presets (section 4.2). `ApiCompareRoutes` returns up to four candidates (one optimal route per preset, de-duplicated, plus bounded DFS alternatives) and scores **all** of them under one chosen preset, so the comparison is fair.

- **Changing weights changes the route.** On C1 Harbor the Safety preset picks a 1 843-unit lap instead of the 1 720-unit lap the other three presets share, and on C3 Downtown 2 080 instead of 1 928. Safety weights difficulty and obstacles most, so it accepts extra distance to use easier roads.
- **Limitations of one weighted scalar.**
  1. The answer depends on the weights *and* on the normalisation. We divide each term by the circuit's mean; dividing by the maximum or the median would shift which route wins at the margin. Neither choice is more "correct".
  2. A weighted sum can only ever return routes on the *convex hull* of the trade-off set. A route that is a good compromise but lies in a "dent" of the Pareto front (for example moderately fast *and* moderately safe) is never optimal for **any** choice of weights.
  3. Summing incommensurable quantities hides information. A cost of 9.2 says nothing about whether the route is short and dangerous or long and safe. That's why the Explorer shows distance, time and fuel next to the scalar.
  4. It also weakens A*: the heuristic must be a lower bound on the *combined* cost, and the combined cost per unit distance varies widely between roads, so λ is small (Experiment 2).
- A Pareto-front search (section 9.3) would avoid points 2 and 3, at the cost of returning a set of routes instead of one.

### Challenge 3 — Efficient live championship analytics: completed

`ApiTopK(k)` returns the top k racers with a **size-k heap** (O(n log k), O(k) memory) under the same comparator as the full sorts. It also reports its comparison count next to a full Merge Sort's and verifies that the result equals the first k entries of the sorted standings.

- **Effect of k relative to n** (Experiment 5, n = 100 000): for k = 1 or 10 the heap does about n comparisons and is about 75× faster than sorting. At k = n/2 it costs more than a full sort (2.5 M vs 1.5 M comparisons), and heapify-then-extract is the better heap method there. At k ≈ n both heap methods do about 1.8 n log₂ n comparisons against Merge Sort's 0.9 n log₂ n.
- **Rule of thumb:** use a size-k heap when k ≪ n (a live top-3 leaderboard) and a full sort when most of the ranking is needed (complete standings). The dashboard shows both.

## 9.2 Backend limitations

- **Weighted scalar cost** (Challenge 2 above). The preset weights are a design choice and were tuned by hand on our three circuits.
- **A*'s heuristic is weak under the game presets.** One global λ (minimum cost per unit of straight-line distance) is a valid but loose bound when costs depend mostly on difficulty and fuel. On long routes A* then expands almost as many vertices as Dijkstra and runs slower, because of the per-push square root (Experiment 2).
- **Knapsack model.** One budget (extra detour seconds) with fixed benefit estimates (2 s per nitro charge, 60 s for a needed station). The benefits are documented estimates, not simulated outcomes. Fuel, time and nitro capacity are handled by separate feasibility rules rather than a true multi-constraint knapsack.
- **Refuelling DP on a fixed route.** It decides where and how much to buy along the route the planner already chose. It doesn't choose the route and the refuelling stops jointly, which could sometimes find a cheaper combination (for example a slightly longer lap that passes a station).
- **Scripted events.** Each circuit plays its authored event script. `Settings.eventSeed` is reserved and currently not used to generate random events.
- **Opponent detours** are limited to the detours documented in each track file (as the brief specifies for Opponent 1). A closure without a documented detour sends Opponent 1 into the wait/DNF recovery policy.
- **Recovery policy** is wait-then-DNF only. A racer never reverses or takes an illegal road, by design, and it can't "push through" a blocked road.
- **Generator.** Layouts come from a jittered grid with 4-nearest-neighbour roads, so generated tracks look similar. All generated roads are undirected (the validator supports one-way roads, but the generator doesn't create them). The checkpoint-spacing rules are fixed constants.
- **Measurements** were taken on one laptop CPU whose clock speed varies under sustained load. We report medians and use operation counters for every complexity claim, and Experiment 2 times its two algorithms in alternating runs, but absolute timings shift: a full re-run of the same binary was 1.5–3× slower on the largest inputs, with identical operation counts. The committed results are from one complete run.
- **Portability.** The benchmark timer uses Windows' `QueryPerformanceCounter` (one isolated file), and the build is tested with MSVC only.

## 9.3 Future improvements

1. **Tighter A* heuristics:** one λ per cost term, or landmark-based (ALT) lower bounds, each with its own admissibility proof. This should recover A*'s advantage under the game presets.
2. **Pareto routing:** a label-setting multi-objective search that returns the non-dominated routes and lets the player pick, instead of fixing weights in advance.
3. **Joint route-and-refuel planning:** a DP over (vertex, checkpoint progress, fuel level) states would choose the route and the stops together.
4. **Seeded random event streams**, using the reserved event seed, while keeping races reproducible.
5. **Generator variety:** other vertex layouts (radial, clustered), one-way roads with directed validation, and difficulty-aware placement.
6. **Prim's MST** as a second backbone algorithm for comparison with Kruskal (the brief requires only one).
