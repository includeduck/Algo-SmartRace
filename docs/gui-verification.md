# GUI verification

Baseline: main b5d9c18788744f6723992cf7c3be4b515818ab63, 17 November 2026.

## Build and execution evidence

- VS18 CMake configure: succeeded.
- Debug: full project build succeeded, including smartrace, smartrace_gui_checks,
  existing CLI and existing backend tests.
- Release: full project build succeeded, including GUI, GUI contract checks,
  CLI and backend tests.
- GUI launch: blocked by Windows Application Control.
- GUI contract-check execution: blocked by Windows Application Control, shell exit 1.
- Existing backend tests/CLI were blocked during the preceding audit. No runtime
  pass is assumed from that audit.
- git diff --check is part of the final static check.
- Final static checks passed: git diff --check, commit-helper PowerShell syntax,
  and inspection for forbidden backend includes/STL in GUI sources. These checks
  do not establish runtime behavior.
- No authored circuit regeneration, backend source modification or dependency
  upgrade beyond the pinned GUI plan was performed.

## Commands

Run from the repository root with Visual Studio's CMake on PATH:

~~~powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
cmake --build build --config Release
.\build\Debug\smartrace_gui_checks.exe
.\build\Debug\smartrace_tests.exe
.\build\Debug\smartrace_cli.exe
Push-Location build/Release
.\smartrace.exe
Pop-Location
~~~

If cmake is not on PATH, use
C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe.

The existing tests and CLI can write generated tracks under ignored build/.
The GUI only writes generated_track.trk when Save is requested in the Lab.

## Manual matrix (pending; do not mark as passed without running)

| Area | Action | Expected result |
|---|---|---|
| Launch | Run beside copied data/, then try missing data | Three circuits load; missing files show a persistent error and retry control |
| Layout | Resize to 1600x900, 1280x720 and 800x600 | Required controls remain reachable; scroll panels do not activate clipped controls |
| Keyboard | Tab/Shift-Tab, Enter, numeric Left/Right, PgUp/PgDn | Visible focus and accessible controls |
| Explorer | All circuits; BFS source=target and reachability-only target -1 | Minimum-road labels and real reachable checkpoints |
| DFS | Inspect cycle witness/components/dead ends | Weak components labelled correctly; witness highlights |
| Routes | Four presets, Dijkstra/A*; compare alternatives | Exact backend scalar; equal costs; unrestricted candidates clearly labelled |
| Resources | Zero and positive budgets; infeasible plan | Distinct fuel/nitro/time units; real choices and error states |
| Race | Quick race; pause/resume, 1 tick, playback speed | Pause freezes time; one tick adds 20 ms |
| Manual | Choose roads; skip/buy fuel; use nitro; wait for reopening | Backend validates commands; invalid choices give errors |
| Session | Race -> Lab -> Back and Race -> Paddock -> Continue | No race restart, lost log, changed autopilot, pause or speed |
| Events | Observe real scripted closures/restrictions and expirations | Road state and logs reflect current graph; clock never restarts |
| Results | Finish/DNF/time-limit records and missing lap | Sentinel times display dashes; every racer appears |
| Championship | Complete circuits 1, 2, 3; attempt repeated Continue | Exactly one record per race and a final champion |
| Rankings | Merge/Quick/Heap and k=1..n | Agreement badge and top-k prefix match backend |
| Lab | All 13 modules, initial/next/back/seek/final/play | Real frames, counters and pseudocode; backward seek restores state |
| Lab resources | Brief example and circuit mode with no items | Backend result identifies actual example; no fake circuit claim |
| Lab tables | Scroll refuelling table to column 1000 | Viewport rendering, readable indices and unreachable-state labels |
| Generator | Seed 2, rejection/acceptance, save/explore | Actual attempts shown; no asserted rejection count; load locked during session |

## Original checkout commit limitation

The working checkout's .git directory is read-only in this session. git add and
git commit failed creating .git/index.lock. No commit was created. An isolated
clone attempt also failed under host process restrictions. No permission bypass
was attempted.

tools/commit-gui.ps1 prepares three named commits from only the GUI-owned paths
when run in a normal local terminal. It refuses to include pre-staged changes and
does not stage the existing codex-plans files or push to a remote.

## Publication checkout

A separate existing checkout at Algo-SmartRace/ is based on the newer backend
Phase 5 commit 4fbf42dd498ed31fc4bab2c2a25b98b95733dc16 and permits Git writes.
GUI changes were copied there, merging only the GUI CMake block, adding the
vendored-library ignore exception, and preserving the latest backend README.
The original parent checkout and its planning files were preserved. The three
GUI commits are created in this publication checkout. The original read-only
Git limitation above does not describe this newer checkout.

The merged publication checkout configured successfully and built smartrace and
smartrace_gui_checks in Debug and Release against Phase 5. Runtime checks remain
unverified due to the earlier Windows Application Control block. A normal Git
push dry run authenticated successfully; the connected GitHub API integration
itself returned 403 for write access. The original checkout was not reset.
