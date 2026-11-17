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
