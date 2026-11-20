# GUI / backend integration handoff

17 November 2026. GUI contributor: Syed Muhammad Rayyan Hassan (24I-3000).
Backend contributor: Muhammad Wasay Tariq (24I-3071).

## Implemented GUI integration

All five modes now have native C++ screens under src/app/. The GUI includes only
api/SmartRaceApi.h from the backend. All graph searches, resource plans,
simulation, traces, sorts and rankings come from that API. Backend source,
authored circuits, CLI and existing tests have not been changed.

- Menu, settings and circuit selection use real load errors and validated input ranges.
- Explorer offers overview/validation, BFS, DFS, ordered lap plans, unrestricted
  alternatives and DP/greedy/refuelling data.
- Racing supports pause, manual/autopilot, outgoing-road commands, station purchases,
  nitro, bounded 20 ms stepping and copied event logs.
- Opening Lab or leaving Race stops simulation stepping. Returning to Race retains
  its session state; only a new race serial resets the controls.
- Results copy backend records before recording a championship race. The GUI checks
  racesCompleted before and after NextRace so a next-circuit load failure cannot
  cause a second award. The dashboard can retry loading the next circuit.
- Dashboard displays backend sort agreement, counters, top-k prefix verification,
  race history and the tie-break criterion.
- Lab supports every LabAlgo module and graph/array/table/event traces, replay,
  seek, pseudocode, counters and generated-track save/explore.

These are source implementation facts. Manual UI behavior remains unverified:
Windows Application Control blocked the executable. Build success is not proof
of interactive acceptance.

## Backend update reviewed before publication

Publication uses backend Phase 5 at 4fbf42dd498ed31fc4bab2c2a25b98b95733dc16,
preserving the partner's source, experiments, measurements and documentation.
The public API signatures and ApiTypes are unchanged. H4 is resolved by wiring
startNitroCharges into the engine. The range-validation part of H5 is resolved;
ApiInit still requires an actual load to establish data availability. H6 now has
bench/ and committed results; their execution was not independently verified
here, and the final technical report remains a separate deliverable. H1-H3 and
H7-H8 remain applicable. The table below retains the original audit evidence;
these Phase 5 resolutions supersede its older missing/ignored descriptions.

## Original contract handoff

Recorded handoff only; no direct message was sent.

| ID | Evidence / GUI behavior | Smallest proposed backend action | Blocks |
|---|---|---|---|
| H1 | ApiCompareRoutes is unrestricted Start-to-Finish. GUI labels it exploration-only and never installs it as a race plan. | Confirm intended semantics; add an ordered-checkpoint comparison only if selectable race alternatives are wanted. | Race-plan alternatives only |
| H2 | ApiPlanLap exposes structured metrics, while arbitrary LabInput endpoint searches expose trace/result text. | Add a structured selected-source/target RouteView query using existing Dijkstra/A*. | Full selected-endpoint route summary |
| H3 | EdgeView has no optimized edge scalar, and RouteCandidateView has no edgePath. GUI displays road distance as distance; candidate selection highlights vertices only. | Add backend edge costs and candidate edge IDs if exact candidate road overlays are required. | Exact per-edge costs/overlays |
| H4 | Settings.startNitroCharges is not passed to RaceEngine. | Document frozen initial 1 or wire the existing setting. GUI preserves it. | No current screen |
| H5 | ApiInit stores the directory; ApiSetSettings assigns without validation. | Correct header documentation or add validation. GUI loads an actual circuit and validates its controls. | No current screen |
| H6 | bench/, raw measurements and technical report are absent. | Supply the five repeated experimental studies and report contributions after GUI, as agreed. Exact two remaining phase scopes still need partner confirmation. | Final experiments/report deliverables |
| H7 | Pickup/refuel Lab rebuilds from Start/full tank on the current graph. | Confirm this baseline context; add a live-context option only if needed. GUI states the baseline. | Live-human resource trace |
| H8 | BuildLabItems falls back to A/B/C if no eligible circuit items exist. | Add structured input-source metadata. GUI prominently displays the backend result text identifying the actual example. | Structured source badge only |

## Verification and next review

Debug and Release build outcomes are recorded in gui-verification.md.
Run smartrace_gui_checks and existing smartrace_tests in an allowed developer
session. Then exercise the manual matrix before marking GUI acceptance complete.
The contract check executable tests public API assumptions; it does not simulate
mouse interaction or replace manual screen checks.

When the partner changes the API, inspect the diff against this baseline and
update only affected adapters/screens. Keep algorithms, race rules, authored
tracks, experiments and backend tests with Person 1. Advanced challenges remain
bonus unless the instructor says otherwise.

## Backend response: API v1.1 (Wasay, 17 Nov 2026)

All open requests are answered in `SMARTRACE_API_VERSION 11`. Everything is
additive: no existing function changed signature or meaning, and new struct
fields are appended, so `= {}` initialisation and the current screens keep
compiling (verified: Debug + Release build, `smartrace_gui_checks` 134/134,
`smartrace_tests` 129/129). Nothing in `src/app/` was edited; wiring these
in is yours.

| ID | Answer | How to use it |
|---|---|---|
| H1 | `RouteCompareView ApiCompareRaceRoutes(Preset evalPreset)` -- candidate **race laps** from the human's position (Start before a race) through every remaining checkpoint in order, one per preset, planned with A* exactly as the autopilot plans them, de-duplicated, scored under `evalPreset`. | Each `RouteCandidateView.sourcePreset` is the `Preset` (as int) that drives it. To make a candidate the race plan: `ApiCmdPreset((Preset)c.sourcePreset)` during a race, or set `Settings.defaultPreset` before `ApiRaceStart`. A test checks every candidate equals `ApiPlanLap(thatPreset, AStar)`. `ApiCompareRoutes` keeps its unrestricted exploration meaning. |
| H2 | `RouteView ApiPlanRoute(int source, int target, Preset, RouteAlgo)` -- one structured search between any two vertices. | Same fields as `ApiPlanLap`. Bad ids or unreachable: `found=false`, `blockedLegIndex=0`, `ApiLastError()` set. `RouteView` now also carries `expanded`, `relaxed`, `heapPushes`, `heapPops` (filled by `ApiPlanLap` too). |
| H3 | `ArrayView<double> ApiEdgeCosts(Preset)` -- per road (index = edge id) the exact cost the routers minimise, on the live graph; `-1` = blocked. `RouteCandidateView` gains `edgePath`. | Summing `ApiEdgeCosts` over a route's `edgePath` equals its `totalCost` (tested). Draw candidates road by road with `edgePath`; `sourcePreset` is `-1` for DFS alternatives. |
| H7 | `LabInput.useRaceState` (default `false`). | `true` during a race makes Knapsack DP, Greedy and Refuel DP plan from the human's live vertex, fuel, nitro and remaining laps -- the same origin `ApiPlanPickups` / `ApiPlanRefuel` use. With no race running it falls back to the baseline and says so. |
| H8 | `LabRunInfo ApiLabRunInfo()` -- `source` (`LabDataSource`: LoadedCircuit, LiveRace, BriefExample, BriefExampleFallback, ChampionshipStandings, RandomKeys, GeneratedTrack, CircuitEvents), a one-line `description`, and the `sourceVertex` / `targetVertex` actually used (defaults resolved). | Show a badge from `source` instead of parsing `ApiLabResultText()`. Valid until the next `ApiLabRun`. |

Also changed since your baseline (v1.0.2, behaviour only): the human
autopilot routes with A* (same optimal cost as Dijkstra), and scripted events
aimed at the wrong kind of element are rejected and logged. The pre-race
baseline for pickups now uses `Settings.startNitroCharges` instead of the
fixed default.


## GUI response: v1.1 integrated (Rayyan, 17 Nov 2026)

All H1–H8 requests above are now resolved in the GUI or supplied partner deliverables. Ordered race alternatives can be selected before/during a race; selected-endpoint searches expose metrics; maps use exact optimized road costs and candidate road IDs; Settings exposes starting nitro; live Lab resources and structured source badges are wired. The event seed is labelled reserved. No further backend API change is requested.

Current verification supersedes the earlier process-blocked audit: Debug/Release backend tests 129/129 (28,480 checks), GUI contracts 238/238 in each, Release application rendering smoke 72/72. The real screen loop completes a championship, tests the 40-second Harbor round trip and captures all modes and Lab view types. Manual input acceptance remains pending; see gui-verification.md and gui-partner-review.md.

## New completeness findings for Person 1 - 19 November 2026

H1-H8 remain closed. The following findings are independent of those API additions.
They were source-derived in the 18 November completeness audit; the current
GitHub comparison confirms no subsequent backend source changes. Happy-path
suite passes do not establish these missing regression cases. No backend-owned
implementation or packaging script has been changed by the GUI work, and this
handoff has not been sent through an external messaging service.

| Finding | Source evidence | Requested change and acceptance |
|---|---|---|
| F01: active cost-change replanning | `src/sim/RaceEngine.cpp`: `Step` sets `m_needsReplan`; only Waiting recovery consumes it. `InvalidateStalePlans` checks blocked roads, not changed scalar costs. | Consume relevant changes at the next safe vertex decision for active autopilot. Test increased oil/restriction cost, cheaper reopened alternative and simultaneous changes; preserve current edge, time, checkpoints, laps and manual/fixed-opponent choices. |
| F02: nitro fuel enforcement | `RaceEngine::TryStartNextEdge` checks baseline fuel. `AdvanceOnEdge` applies boosted fuel, clamps a negative tank and still advances the full distance. `CmdActivateNitro` does not validate feasibility. | Specify and implement a fuel-valid mid-edge boost/exhaustion policy. Example: 100 tenths available, baseline edge needs 100, but the boosted traversal needs about 200. Test exact baseline sufficiency, mid-edge activation/expiry, zero fuel, fractional rounding, cost changes and determinism; never allow travel paid by discarded negative fuel. |
| F03: overlapping service effects | `RaceEngine::OnEvent` unconditionally reactivates a station on an old expiry; booster expiry unconditionally disables it and one stored expiry slot is overwritten. | Track independent lifetimes and preserve base availability. Outages [0,30s] and [10,40s] must keep a station unavailable at 30s. Booster intervals [0,25s] and [10,35s] must survive the first expiry. Define collection semantics; test stale expiry after collection/new activation and same-time order. Preserve oil/speed overlap behavior. |
| F04: physically feasible pickups | `src/sim/ResourcePlanner.cpp::BuildPickupItems` prices time-reachable detours without actual remaining fuel or validating their reconstructed checkpoint traversal. Public API/Lab reuse this builder. | Reuse backend fuel/route helpers to filter and validate individual and combined chosen detours from the actual origin/resources. Test insufficient fuel on access leg, inactive service, checkpoint-order violations and combined choices. Retain scalar DP/greedy units; compare DP and exhaustive search on the same filtered problem. |
| F05: numeric import boundary | `src/io/TrackLoader.cpp` parses EDGE fields and calls `Graph::AddEdge` without finite/nonnegative/range checks. Validation can reach routing first; `ApiRaceStart` does not gate on validator success. | Reject malformed/nonfinite/negative distance/time/fuel and difficulty outside 1..5 before graph algorithms, with line/field errors. Preserve supported zero values. Reject or prevent starting unplayable imports. Keep the GUI load-error/retry contract. |
| F09: omitted package docs | `tools/package.ps1` staging `$folders` omits `docs`, while README links three GUI documents. Existing ZIP contains no docs entries. | Include intended user-facing docs. Run `python src/app/checks/CheckSubmission.py build/package/24I-3071_24I-3000_BSE-5B.zip --source-root .` after final PDF/package creation, in addition to `-Verify`. It checks CRC, required membership, local Markdown links, runtime data and reviewed source/report/result bytes. |

F01-F05 need backend-owned fixes and regression tests before final submission.
F02 exhaustion policy and F03 collection semantics need an explicit backend
decision. F06/F08 final artifacts depend on those fixes, actual F07 physical
acceptance, the latest remote report/diagram, final PDF review and joint package
verification. A new API version or a GUI-side algorithm duplicate is not needed
just to route these findings.

F10 is corrected in GUI-owned code: `ui/LabEventStatus.h` derives display status
from the recorded frame, insertion sequence and applied/cancelled IDs.
`checks/GuiChecks.cpp` compares each real event operation on all three circuits
against the labels and replays every frame backwards. The renderer no longer
labels future rows as scheduled. Current command outcomes are recorded in
`gui-verification.md`.

## Backend response: F01–F05 and F09 fixed (Wasay, 20 Nov 2026)

All six findings were confirmed in the source and are fixed, each with
regression tests in `tests/test_completeness.cpp` and
`tests/test_completeness_api.cpp` (18 tests). Every engine-level test was
checked to fail with its fix reverted. No API change: version 11.
Debug/Release: `smartrace_tests` 147/147 (28,554 checks), no CRT leak report;
`smartrace_gui_checks` 370/370. `smartrace_gui_smoke` was not re-run on the
backend side.

| Finding | Resolution | Tests |
|---|---|---|
| F01 | `RaceEngine::TryStartNextEdge`: after any graph change, the human autopilot and Opponent 3 drop their plan at the next vertex decision and replan (A* / their strategy). The current edge, time, checkpoints and laps are untouched. Manual driving is untouched. Opponents 1 and 2 keep their fixed preference and still replan only when a planned road closes (`InvalidateStalePlans`). | `F01_OilSpillAhead_…`, `F01_ReopenedCheaperRoad_…`, `F01_NoGraphChange_…_FixedOpponentKeepsItsRoute` |
| F02 | **Policy (decision):** a boosted tick is allowed only if fuel left afterwards, minus the not-yet-deducted remainder, covers the rest of the current road at the normal rate. Otherwise the nitro is held for that tick: normal speed, normal fuel, and `nitroTimeLeftMs` does not run down. The held state is `Racer.nitroHeldForFuel`, logged once per hold. This keeps "available ≥ normal-rate need for the rest of the road" on every tick, so the zero clamp is unreachable. Activation keeps its existing checks: holding makes any activation safe. | `F02_Nitro_NeverSpendsFuelTheTankDoesNotHave` (per-tick invariant), `F02_HeldNitro_KeepsItsRemainingTime`, `F02_AffordableNitro_StillBoostsAndExpires` |
| F03 | **Policy (decision):** stations keep an outage counter, so each outage's reactivation decrements it, and at zero the station returns to its base (as loaded) availability. Each booster is its own (vertex, expiry event, expiry time) record. A pickup is lit while it has any booster or an uncollected base charge. Collecting takes one charge: the soonest-expiring booster (its expiry is cancelled, so no stale expiry), else the base charge. Oil and speed overlap behaviour is unchanged. | `F03_OverlappingStationOutages_…` ([0,30 s] + [10,40 s] → closed at 30 s), `F03_OverlappingBoosters_…` ([0,25 s] + [10,35 s]), `F03_CollectingOneBooster_…_StaleExpiryIsCancelled`, `F03_SameTimeOutageAndReactivation_…` |
| F04 | `BuildPickupItems` now takes the actual fuel and last completed checkpoint. A detour is offered only at insertion points where it is driveable: a tank simulation from the actual fuel, topping up the per-stop maximum at active stations on the route, never runs dry (or, if the plain route already runs dry, lasts past the detour), and checkpoint progress is unchanged. New `PickupSelectionFeasible` re-checks a chosen set as one route; `ApiPlanPickups` withdraws the most fuel-adding chosen item and re-solves until DP and greedy selections are both driveable. DP and greedy, and DP versus exhaustive, compare on the same filtered items. Scalar units are unchanged. The Lab uses the same filtered items. | `F04_DetourBeyondTheTank_IsNotOffered`, `F04_InactivePickup_IsNotOffered`, `F04_TwoDetoursThatFitAlone_DoNotFitTogether`, `F04_DetourThatWouldSkipCheckpointOrder_IsRejected`, `F04_Api_RepairsAnInfeasibleCombination` |
| F05 | `TrackLoader`: strict whole-token parsing for VERTEX x/y/cpOrder and every EDGE number. Values must be finite with magnitude ≤ 1e9; distance, baseTime and fuelCost ≥ 0 (zero allowed); difficulty an integer 1–5; shortcut and directed 0 or 1. Errors name the line and field. `ApiRaceStart` refuses a circuit whose validation failed (`Infeasible`, "circuit failed validation: …"); load and explore still work, so the GUI load-error/retry contract is unchanged. | `F05_Loader_RejectsBadEdgeNumbers_…`, `F05_Loader_KeepsZeroAndBoundaryValues`, `F05_Api_ACircuitThatFailsValidation_CannotBeRaced` |
| F09 | `tools/package.ps1` stages `docs/`. Run `CheckSubmission.py` after the final package as you proposed. | — |

Report chapters 04, 05, 06, 07 and 09 describe the new rules and list these as
bugs found by review; the PDF was regenerated.
