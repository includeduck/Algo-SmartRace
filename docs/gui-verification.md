# GUI verification — 17 November 2026

Integration baseline: main 16c0304a83a85da54ae74c48fb29764093dfdcd2, API v1.1. The earlier publication audit could only compile due to process restrictions. This review supersedes that runtime limitation: native builds, checks and the application's renderer now executed successfully in build/partner-review. The original root checkout remains preserved.

## Executed results

| Check | Debug | Release |
|---|---|---|
| Full x64 MSVC build (19.50; Windows SDK 10.0.26100) | Passed | Passed |
| Existing backend tests | 129/129; 28,480 checks; 0 failures | 129/129; 28,480 checks; 0 failures |
| GUI contract checks | 238; 0 failures | 238; 0 failures |
| Application rendering smoke | Built | 72; 0 failures |

Backend Debug tests emitted no CRT leak report. This is not a full interactive GUI memory-leak audit. The partner's five experiment datasets/charts were preserved; this GUI review does not claim a fresh full benchmark run.

The rendering smoke test instantiates the actual App, queues the same screen actions used by controls, runs real backend simulation and captures its actual OpenGL framebuffer. It does not automate the desktop or test physical keyboard/mouse dispatch. Screenshots under report/screenshots include the five modes, race results, graph/table/array/event Lab views, generator, a genuine infeasible live resource result and 800×600 layouts. Windows display scaling was 125%. The screenshot review found and fixed a resize viewport/scissor mismatch and clipped-control fragments; the smoke also checks framebuffer coverage.

The smoke verifies all 13 Lab modules and initial/final seeking, the 40-second Harbor event followed by live Explorer and Lab round trips, preserved paused time, a three-race championship and duplicate-Continue rejection, and seed 2's rejected first / accepted second generator attempt.

## Reproduce

Run from repository root; the smoke writes/refreshes report/screenshots/*.png:

~~~powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug -- /m:1 /nr:false /p:TrackFileAccess=false /p:PreferredToolArchitecture=x64
cmake --build build --config Release -- /m:1 /nr:false /p:TrackFileAccess=false /p:PreferredToolArchitecture=x64
.\build\Debug\smartrace_tests.exe
.\build\Release\smartrace_tests.exe
.\build\Debug\smartrace_gui_checks.exe
.\build\Release\smartrace_gui_checks.exe
.\build\Release\smartrace_gui_smoke.exe
python report/build_report.py
~~~

The explicit MSBuild properties avoid a file-tracker hang in this automation host. A normal local CMake build can omit them. Native game launch is build/Release/smartrace.exe, with data/ beside the executable; CMake copies it. The report exporter additionally needs Python, ReportLab, Pillow and the Windows Arial fonts. These are report tools, not game dependencies.

## Acceptance status

| Area | Automated evidence | Still manual |
|---|---|---|
| Data loading | Three real circuits validated; missing-data API recovery contract | Click retry in missing-data screen |
| Routes | Preset cost equality, endpoint edge cases, exact scalar sums, candidate road IDs; real screen selection action | Click/Shift-click endpoints and inspect all candidates |
| Race | Time step contract, live planner/Lab freeze and pause retention, three completed races | Road choices, wait/reopening, station purchases, nitro using physical controls |
| Resources | Structured example/live metadata; real DP table and infeasible live trace | Scroll wide refuelling tables and inspect all budgets |
| Championship | Exactly once progression, final champion, sort/top-k agreement | Review every sort control visually |
| Lab | All modules expose real frames; initial/final seek; seed-2 rejection/acceptance | All intermediate playback speeds, drag seeking and save/explore click flow |
| Layout | 1600×900 and 800×600 framebuffer captures inspected at 125% scale | Other scales, monitors and 1280×720 |
| Accessibility | Visible-control clipping and focus implementation reviewed | Tab/Shift-Tab, text entry, focus order and usability |

Do not mark this remaining column passed without performing those interactions. The original H1-H8 API dependencies are closed; the later completeness findings below remain open. Historical extraction/rebuild evidence is recorded in gui-partner-review.md and does not establish that the current source has been packaged.

## Completeness follow-up - 19 November 2026

Local source baseline: `main`, `2668111964365c784a36914c78f9bd1343d19b3f`,
with the GUI changes listed in `gui-implementation-progress.md`. GitHub main was
checked through the connector at `ed06ab875383d034288745b2056e808119f44283`:
three commits ahead, seven report/diagram/handoff paths changed, no game/backend
code changes. `git pull --ff-only` failed with `getaddrinfo() thread failed to
start`; a normal archive download also failed DNS resolution. No remote file
was overwritten and the dirty outer checkout was preserved. This is a verified
game-code baseline, not a successful synchronization of the latest report.

### Fresh command results

| Command/check | Observed outcome |
|---|---|
| CMake full Debug build with documented MSBuild properties | Exit 0; all targets built |
| CMake full Release build with the same properties | Exit 0; all targets built |
| Debug/Release `smartrace_gui_checks.exe` | Exit 0; 370 checks, zero failures in each (132 added checks) |
| Debug/Release `smartrace_tests.exe` | Exit 0; 129/129 tests, 28,480 checks, zero failures in each; no Debug CRT leak report observed |
| Updated Release `smartrace_gui_smoke.exe` | Exit 0; 78 checks, zero failures; actual App completed the three-race championship |
| Archive contents checker against the existing ZIP | Exit 1 as expected; CRC valid, linked docs/remote diagrams absent and current source/binary correspondence fails |
| Archive checker synthetic verification | Three in-memory cases passed: valid membership, broken link detection, changed runtime-data detection; no submission artifact created |
| `git diff --check` | Exit 0 |

Compiler tools on this host: MSVC 14.50.35717 (compiler 19.50); the partner's
recorded results use compiler 19.51. Neither compiler environment is claimed
to prove physical input acceptance or the backend cases in the new handoff.

The F10 regression consumes actual EventQueue Lab frames for all three circuits.
It checks every schedule/cancel/apply transition against the display labels,
verifies insertion sequence maps to recorded PushHeap order, and seeks backwards
through every frame to the initial state. No separate event queue is simulated
by the GUI. The actual renderer shows every initial row as NOT YET SCHEDULED,
then only event #0 as QUEUED after the first push. Returning one step restores
the initial labels. Time, priority and insertion sequence remain visible.

Rendering smoke was run from isolated `build/verification-20261119`, with its
own copy of data. The final log is `build/verification-20261119/smoke.log`;
captures are under that folder's `report/screenshots/`. Images
`14_lab_events_initial.png` and `15_lab_events_first_push.png` were visually
inspected. `16_lab_events_rewound.png` records the backward step. Original
report screenshots and measured experiment files were not replaced. An initial
smoke invocation used the wrong relative executable path and did not launch;
the corrected command used `../Release/smartrace_gui_smoke.exe` and passed.
The first successful run preceded the new capture build (72 checks); the final
rebuilt run above is the current 78-check result.

### Remaining acceptance and artifact gates

Every physical interaction in the Still manual column remains pending. Native
desktop input tooling is unavailable in this session. No OS security or display
scaling setting was changed, and no input pass is inferred from screenshots or
Ui::PostAction. Run those interactions after F01-F05 are fixed, including
missing-data Retry using an isolated copy, manual road/fuel/nitro/wait controls,
1280x720, 100%/150% display scaling, keyboard/text focus and generated-track
save/explore/play.

The latest remote report corrections and architecture diagram were inspected,
but cannot be synchronized locally while Git/network access fails. The PDF was
not regenerated from stale sources. The old archive was not overwritten or
submitted. Its final replacement depends on backend fixes, actual manual
acceptance, report synchronization/final review, the Person 1 docs-inclusion
change and clean-folder packaging verification. Benchmark timings were not rerun.
