# 3. System Architecture and Game Flow

## 3.1 Module boundaries

| Layer | Main files | Responsibility |
|---|---|---|
| Application and session | src/app/App.*, AppState.h, Screen.h | Screen ownership, navigation, race serial, copied results and selected preset |
| Screens | src/app/screens/ | Menu/settings, circuit selection, Explorer, Racing, Results, Championship and Lab |
| Presentation utilities | src/app/ui/ | Shared theme, focus, scroll clipping, formatting, map projection and trace renderers |
| Public boundary | src/api/SmartRaceApi.h, ApiTypes.h | Typed borrowed views, commands, error status and structured source metadata |
| Backend | src/core, model, algo, sim, io | Graph, handwritten algorithms, race engine, effects, resources and championship |
| Verification and experiments | tests/, src/app/checks/, bench/ | Correctness, GUI contract/rendering checks and repeated measurements |
| Assets and delivery | data/, third_party/, results/, report/ | Circuits, pinned GUI dependencies, measurements and report sources |

The GUI includes only api/SmartRaceApi.h from the backend. It transforms
coordinates for drawing and handles input and presentation state. It does not
implement routing, resource planning, race scoring or sorting.

The GUI, CLI, tests and benchmarks link smartrace_core. Lab runs attach a trace
recorder to those same algorithm functions. TraceCursor reconstructs a selected
operation for visualization, including backward seeking.

![Figure 3.1 - System architecture. Arrows follow the real `#include` and link dependencies; the GUI and CLI reach the backend only through the one public API header. Source: `report/diagrams/architecture.puml` (PlantUML). Verification counts inside the diagram are the 17 November snapshot; current results are in Section 7.5.](diagrams/architecture.png)

## 3.2 Screen responsibilities

| Required mode | Implemented behavior | Main API calls |
|---|---|---|
| Main Menu | Championship, Quick Race, Lab, instructions, settings, existing standings | ApiInit, ApiLoadCircuit, ApiGetSettings, ApiSetSettings, ApiChampionshipStart |
| Track Explorer | Map/validation, BFS, DFS, ordered lap and selected-endpoint routing, alternatives, resources | ApiGetGraph, ApiValidateTrack, ApiRunBfs, DFS queries, ApiPlanLap, ApiPlanRoute, ApiCompareRaceRoutes, ApiCompareRoutes, ApiEdgeCosts, resource planners |
| Racing | Actual car positions, resources, manual commands, pause, speed, events, live planner and Lab | ApiRaceStep, ApiRaceSnapshot, ApiRaceResults, ApiRaceLogSince, ApiCmd* |
| Algorithm Lab | Thirteen modules, four renderer kinds, replay, seek, counters, pseudocode and actual input-source badge | ApiLabRun, ApiLabFrame, ApiLabGraph, ApiLabRunInfo and Lab metadata queries |
| Championship Dashboard | Results, standings, sort agreement, counters, top-k, history and tie criterion | ApiChampionshipNextRace, ApiStandings, ApiTopK, ApiRaceHistory, ApiTieBreakReason |

Several supporting screens separate settings, instructions, circuit selection
and results while keeping all five required modes reachable.

## 3.3 Navigation and session lifecycle

Quick Race follows Menu -> Circuit Select -> Explorer -> Race -> Results.
Championship starts at circuit zero, then repeats Explorer -> Race -> Results
-> Dashboard for each of the three circuits. Continuing from Results records
one race and loads the next circuit. The dashboard declares the backend's
champion after the third race.

Lab may be opened from Menu, Explorer or Race. Opening it during a race stops
simulation stepping. Back returns to the recorded origin screen. RaceScreen
resets its controls only for a new race serial, so returning from Lab or the
live Explorer preserves pause, speed, autopilot and the event log.

Circuit replacement, generation loading and race-affecting settings are locked
while a live race or unfinished championship must be preserved. A failed load
drops invalid views and shows an actionable error. Results are copied before
a circuit change. Championship progress is checked before and after recording
so that a successful award followed by a load failure cannot be awarded twice.

## 3.4 Time, ownership and errors

Rendering uses wall-clock frame time only to accumulate whole 20 ms simulation
ticks. The accumulator bounds catch-up after a long window stall. Pause and
decision dialogs make no ApiRaceStep calls; resuming discards stale elapsed
pause time. Playback speed changes the number of ticks, not the race rules.

Every API view is borrowed. The GUI either consumes it before the next
documented invalidating call or copies the values it must keep. Comparing
Dijkstra and A* copies the first path before the second planner call. Event
log strings and race results are copied. Exact edge costs are refreshed after
simulation stepping, because graph events can invalidate them.

ApiStatus drives errors, and ApiLastError is copied before another API call.
Fuel uses tenths of a unit, race times use milliseconds, and pickup budgets use
tenths of a second. Missing lap and unfinished-result sentinels become dashes
rather than implausibly large or negative times.

## 3.5 API v1.1 integration

Ordered race alternatives preserve remaining checkpoints and carry a
sourcePreset. Applying one selects that preset through settings before a race
or ApiCmdPreset during one. Unrestricted Start-to-Finish alternatives remain
labelled exploratory and cannot be applied as race laps.

ApiPlanRoute supplies structured selected-endpoint paths and counters.
ApiEdgeCosts supplies the same scalar minimized by routing; the map never
mislabels raw distance as optimized cost. Candidate overlays use edgePath,
including on networks with parallel roads.

LabInput.useRaceState requests the human's live resources while a race runs.
ApiLabRunInfo identifies whether the backend actually used live state, baseline
circuit data, the brief example, an example fallback, championship records,
random keys, generated track data or circuit events. The GUI does not infer
that source by parsing result prose.

## 3.6 Visualization and accessibility limits

The map fits graph coordinates, supports bounded zoom and pan, and distinguishes
checkpoint/service roles, one-way roads, blocked roads and restrictions. Tables
and event lists render a viewport of large datasets. Smaller desktop windows
use scrollable panels; keyboard focus and numeric controls are provided.

The application-level rendering smoke test captures genuine screen output and
checks navigation/session behavior. It is not a mouse/keyboard usability study.
Manual accessibility, low-resolution interaction and station/refuelling
acceptance still require the documented manual matrix.
