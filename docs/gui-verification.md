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

Do not mark this remaining column passed without performing those interactions. No known open backend dependency remains. The final submission archive's extraction/rebuild evidence is recorded in gui-partner-review.md after packaging.
