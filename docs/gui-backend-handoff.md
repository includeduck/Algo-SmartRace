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
