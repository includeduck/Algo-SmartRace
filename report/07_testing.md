# 7. Testing Strategy, Test Cases and Results

*Backend section — Muhammad Wasay Tariq (24I-3071). GUI testing is in Syed Muhammad Rayyan Hassan's (24I-3000) section.*

## 7.1 Strategy

- **Unit and integration tests** (`tests/`, executable `smartrace_tests`): a small framework written for the project, with no STL. `TEST(name)` registers a function and `CHECK` / `CHECK_EQ` record failures with file and line. The exit code is the number of failed checks. **129 tests, 28 480 checks, all passing** on both the Debug and Release builds.
- **Oracles instead of hand-computed answers** wherever possible: DP against exhaustive search, A* against Dijkstra, BFS against a brute-force relaxation, DFS components against union-find, every sort against the other two and against the comparator, top-k against the sorted prefix, the event heap against a merge-sorted reference. The same oracles run again on large inputs in the experiments (section 8): 248 more checks, all passing.
- **Determinism tests:** the same race run twice gives the same result hash (engine level and through the API), and the same generator seed gives the same track.
- **API robustness tests:** every call before `ApiInit` and with nothing loaded; bad ids and paths; illegal human commands; snapshot invariants on every step of a full race; settings validation.
- **Memory:** Debug builds of the tests and the CLI enable the MSVC CRT debug heap's leak report at process exit. Both runs report no leaks. We confirmed the check fires by planting a deliberate 28-byte leak once.
- **Coding-rule scan** before each commit: no STL headers, no `std::`, no smart pointers, no `qsort`/`bsearch`/`atoi`, no same-line or unbraced bodies (Allman style).

## 7.2 Coverage of the brief's minimum list (section 9.1)

| Area | Required case | Test(s) |
|---|---|---|
| Graph | single vertex | `Bfs_SingleVertexGraph` |
| | disconnected | `Dfs_Components_DisconnectedGraph`, `Kruskal_DisconnectedGraph_ReturnsForest` |
| | cyclic | `Dfs_DetectsCycle_UndirectedTriangle`, `Dfs_DirectedCycle_Detected` |
| | branching / dead end | `Dfs_DeadEnd_SpurOffTheMainLoop`, `Dfs_DeadEnd_UnreachableVertex_ViaOneWayRoad` |
| | directed vs undirected | `Graph_DirectedEdge_OneWayOnly`, `Dfs_DirectedAcyclic_NoCycle`, `Graph_AddVertexAndEdge_Undirected` |
| | block/unblock without rebuild | `Graph_SetEdgeStatus_NoRebuild` |
| Routing | source = target | `Dijkstra_SourceEqualsTarget` |
| | unreachable target | `Dijkstra_UnreachableTarget`, `Bfs_UnreachableTarget_NoCrash` |
| | blocked edge | `Dijkstra_BlockedEdge_Excluded_RouteRecalculates`, `Bfs_BlockedEdgeExcluded` |
| | equal-cost paths | `Dijkstra_EqualCostPaths_DeterministicAcrossRuns` |
| | zero-weight edges | `Dijkstra_ZeroWeightEdge_NoCrash` |
| | graph change during a race | `RaceEngine_RoadClosure_ReroutesWithoutRestartingRace` |
| | A* = Dijkstra | `AStar_MatchesDijkstraCost_OnAllCircuitPresetCombinations`, `AStar_ZeroHeuristicFallback_StillCorrect` |
| | checkpoint order never skipped | `RoutePlanner_PreservesCheckpointOrder`, `RaceEngine_CheckpointsVisitedInOrder_AutopilotNeverSkipsOne`, `ShortcutLoverStrategy_NeverTakesAShortcutThatSkipsACheckpoint` |
| Resources | insufficient fuel | `Refuel_InsufficientFuel_NoStation_Infeasible`, `RaceEngine_InsufficientFuel_NoStation_EveryoneDnfs` |
| | full tank | `Refuel_FullTank_RouteFitsWithoutStopping` |
| | unavailable station | `Refuel_UnavailableStation_CannotBuy` |
| | capacity limit | `Refuel_CapacityAndPurchaseLimit_ForceTwoStops` |
| | zero-cost pickup | `Knapsack_ZeroCostItems_AlwaysTakenWhenUseful` |
| | infeasible plan | `Refuel_FirstLegTooLong_InfeasibleEvenWithStationsLater` |
| | brief example, two greedy-worse cases, DP = exhaustive | `Knapsack_BriefExample_GreedyWorseThanDp`, `Knapsack_TeamCaseG1_…`, `Knapsack_TeamCaseG2_…`, `Knapsack_DpMatchesExhaustive_On500RandomInstances` |
| | recommendations feasible in gameplay | `RaceEngine_LowFuel_DetoursToStation_RefuelsAndFinishes` |
| Events | simultaneous events / deterministic ties | `EventQueue_OrdersByTriggerTimeThenPriorityThenSequence`, `EventQueue_SimultaneousEventsSameTypeTiebreakBySequence` |
| | cancellation | `EventQueue_Cancel_LazyDeletion_SkippedWhenPopped`, `EventQueue_CancelAfterFiring_IsHarmless`, `RaceEngine_NitroCollection_CancelsPendingExpiryEvent` |
| | expiration / overlapping effects | `EffectManager_OverlappingOilSpills_ExpiringOneKeepsTheOtherActive`, `EffectManager_SpeedRestriction_TakesTheStrictestCap` |
| | missing / wrong targets | `RaceEngine_EventWithWrongKindOfTarget_IsRejected` |
| Sorting | empty / singleton | `Sorting_EmptyAndSingleton_NoCrash` |
| | random, sorted, reverse, duplicate-heavy | `Sorting_AllThreeAgree_Random`, `…_SortedAndReverse`, `…_DuplicateHeavy`, `Sorting_Ints_AllMethodsSortDuplicates` |
| | same comparator, racer-id tie-break | `Comparator_FullTieBreakChain`, `Sorting_RacerIdTieBreak_AllMethodsAgree` |
| | top-k = prefix | `TopK_EqualsPrefixOfFullSort_BothMethods`, `TopK_KLargerThanN_ReturnsAll` |
| Integration | complete race, multiple laps | `RaceEngine_FullRace_AllRacersFinishOrAreClassified`, `Api_Snapshots_StayConsistentAcrossSteps` |
| | ordered checkpoints | `RaceEngine_CheckpointsVisitedInOrder_AutopilotNeverSkipsOne` |
| | DNF | `RaceEngine_InsufficientFuel_NoStation_EveryoneDnfs` |
| | road closure | `RaceEngine_RoadClosure_ReroutesWithoutRestartingRace` |
| | championship standings | `Championship_AccumulatesPointsWinsPositionsAndBestLap`, `Championship_ThreeSortsAgree_TopKMatchesPrefix_ChampionDeclared`, `Championship_UnfinishedFirstPlaceIsNotAWin` |
| | determinism | `RaceEngine_SameSeedTwice_ProducesSameDeterminismHash`, `Api_SameSettingsTwice_SameResults` |

**Further groups:** data structures (14 tests: growth, wrap-around, heap order and heapify, union-find, pool, string, deterministic RNG, log ring buffer); the three opponent strategies (8); track generation and validation (9: Kruskal, the validator accepting all three circuits and rejecting deliberately broken ones with the right reason, generation across seeds, determinism, impossible parameters, write/read round trip); the Algorithm Lab (9: trace replay of BFS, of each sort and of the knapsack table matches the real result; seeking backwards equals seeking forwards; every Lab module runs; event-queue pop order; a generated track saves, reloads and validates); the API (12: 7 for v1.0 plus 5 for the v1.1 additions).

## 7.3 Results

| Build | Tests | Checks | Failures | Leaks |
|---|---|---|---|---|
| Debug (x64, MSVC 19.51) | 129 | 28 480 | 0 | none reported |
| Release (x64, /O2) | 129 | 28 480 | 0 | (not instrumented) |
| Experiments' correctness checks (Release) | — | 248 | 0 | — |

Run them with `build/Debug/smartrace_tests.exe` (or `build/Release/…`) from the repository root.

## 7.4 Bugs the tests and experiments found

Testing paid off. These defects were found by tests or experiments during development and fixed:

- **Quick Sort pivot** (Experiment 5): fixed-position median-of-three degraded on sorted and reversed input after the 3-way partition (22 M vs 1.9 M comparisons at n = 100 000). Fixed with seeded sampling; see section 5.12.
- **Recovery timeout reset:** a racer that re-tried a failing plan restarted its 60 s DNF clock on every retry and could wait forever. The original wait start is now kept across retries.
- **Fuel accounting:** rounding fuel per tick lost fractions over a long edge. Fuel is now charged with a fractional accumulator, so a whole edge burns exactly its rule amount.
- **Shortcut legality:** an early version of Opponent 2 could take a shortcut that bypassed a mandatory checkpoint. Shortcuts are now accepted only if they skip plain intersections.
- **Settings:** `startNitroCharges` was ignored and out-of-range settings were accepted. Both fixed in API v1.0.1.
- **Event targets:** a station-outage event aimed at a non-station vertex used to deactivate it. Targets are now checked for the right kind.


## 7.5 GUI integration review — 17 November 2026

GUI section by Syed Muhammad Rayyan Hassan (24I-3000).

| Review check | Configuration | Observed result |
|---|---|---|
| Existing backend suite | Debug and Release, MSVC 19.50 x64 | 129/129 tests, 28,480 checks, zero failures in each |
| GUI contract executable | Debug and Release | 238 checks, zero failures in each |
| Actual application rendering smoke | Release, Windows 125% display scaling | 72 checks, zero failures |
| Compile every target including GUI and smoke | Debug and Release | Passed |

The GUI contract checks cover borrowed-path copies, DNF time formatting, bounded deterministic tick accumulation, real circuit validation, same-endpoint and invalid endpoint searches, equal Dijkstra/A* costs, exact candidate road identities, scalar label sums, structured Lab input-source badges, live resource queries and seeking without advancing race time. Championship checks cover all racers, three circuits, comparator agreement, top-k prefixes and duplicate recording.

The rendering smoke test creates the actual App and queues the same actions consumed by screen Update methods. It advances Harbor beyond the 40-second closure, opens the live Explorer, applies a real preset candidate, visits every Lab module, returns with the same paused race clock, then completes a three-race championship. Seed 2 visibly rejects attempt 0 and accepts attempt 1. Real framebuffer captures include all five modes, results, generator and the graph, table, array and event renderers. A separate capture preserves a genuinely infeasible live resource result rather than replacing it with fabricated values.

Screenshot inspection exposed a Windows DPI resize mismatch in raylib 5.5's logical viewport and scissor dimensions. The GUI now derives its canvas from the actual native window client size and uses matching projection, clipping and mouse scaling. The smoke checks framebuffer coverage; 1600 by 900 and 800 by 600 layouts were inspected. Controls outside a scroll panel's visible area no longer leave disabled fragments or accept focus.

These are automated application checks, not physical mouse/keyboard tests. Manual driving, focus order, text entry, intermediate playback/drag seeking, large refuelling-table scrolling, other display scales and the generated-track save/explore interaction remain in docs/gui-verification.md. No manual usability pass is claimed. Backend Debug tests emitted no CRT leak report; the GUI has not had a full interactive leak audit.


## 7.6 Submission archive verification

The unchanged partner packaging script was run with -Verify from a short clean Windows extraction path. It built every Release target, passed 129 tests (28,480 checks), staged the sources/dependencies/data/results/report and binaries, created the ZIP, then extracted that archive into a fresh folder and rebuilt/passed the same tests. The first attempt in the deeply nested working path failed compiler discovery; shortening the extraction path resolved it. The host also required CMake on PATH, PATHEXT and the documented MSBuild file-tracker properties. The packaged executable has data/ next to it. Archive integrity is checked again after report regeneration; manual gameplay acceptance remains separate.
