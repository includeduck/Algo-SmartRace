# Smart Racing Championship

## Native GUI

The Windows x64 GUI now lives in `src/app/` and links the backend exclusively
through `api/SmartRaceApi.h`. It includes the Main Menu, Track Explorer, Racing,
Algorithm Lab and Championship Dashboard. raylib 5.5 and raygui 4.0 are vendored
under `third_party/` with their licenses; the configured build needs no download.

Build with Visual Studio 2026/MSVC and CMake 3.20+:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
cmake --build build --config Release
Push-Location build/Release
.\smartrace.exe
Pop-Location
```

Run from the executable directory (data is copied there by the build), or the
repository root. Missing data displays an error and retry control. Release has
no console window; Debug keeps its console for diagnostics.

Controls: Tab/Shift-Tab focuses controls; Enter/Space activates buttons;
Left/Right changes a focused numeric control. Mouse wheel scrolls panels, and
PgUp/PgDn scrolls long details. On the map, wheel zooms, middle-drag pans and F
fits. In BFS, click a target or Shift-click a source. During racing, P pauses,
A toggles autopilot, N activates nitro and L opens the Lab without advancing
simulation time. Manual controls let the backend validate outgoing roads and
fuel purchases. Use Wait to let events progress while waiting for a road.

The Lab arrows select all 13 modules. Run records the real backend trace;
playback/seek, pseudocode and counters inspect it. Table row/column controls
expose large DP tables. Generator Save writes `generated_track.trk` in the
working directory; loading it is locked during a live race/championship.

GUI contract checks build as `smartrace_gui_checks.exe`; run from the repository
root. See [verification](docs/gui-verification.md) for the pending manual matrix
and [backend handoff](docs/gui-backend-handoff.md) for exact remaining contracts.
Debug/Release compilation does not establish interactive correctness. Windows
Application Control may require running from an allowed local developer session.

Acknowledgements: [raylib](https://github.com/raysan5/raylib) and
[raygui](https://github.com/raysan5/raygui), by Ramon Santamaria and contributors,
are used under their bundled zlib/libpng licenses. The GUI uses procedural
shapes and the library's bundled font; no external art assets were added.

## Backend reference

Design and Analysis of Algorithms project. This repository currently holds
the **backend** (Muhammad Wasay Tariq, 24I-3071): the data model, all 12
mandatory algorithms, the deterministic race engine, the championship, the
experiments and the tests. The GUI (Rayyan, 24I-3000) is a separate target
that links this backend through one header — see
[`src/api/SmartRaceApi.h`](src/api/SmartRaceApi.h).

The full design is in `impl_plan.md` (not committed — see your own copy).

## Requirements

- Visual Studio 2026 (or any MSVC toolset) with the "Desktop development
  with C++" workload, x64.
- CMake 3.20+.

## Build

```bash
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
```

This produces, under `build/`:

- `smartrace_core` — the backend static library.
- `smartrace_tests.exe` — unit + integration tests.
- `smartrace_cli.exe` — a headless driver that exercises the API (also a
  runnable usage sample for the GUI side).
- `smartrace.exe` — the GUI, once `src/app/` exists.

## Run

Run both from the repository root (they read `data/`):

```bash
build/Debug/smartrace_tests.exe
build/Debug/smartrace_cli.exe
```

The CLI prints the Explorer data for each circuit, plays a full
championship, runs every Algorithm Lab module, then generates a track,
saves it to `build/generated_demo.trk`, reloads it and validates it.

## Experiments (`smartrace_bench`)

The five required experimental studies (Project.pdf section 7). Build
**Release** and run from the repository root (Experiment 2 reads
`data/circuits`):

```bash
cmake --build build --config Release
```

```bash
build/Release/smartrace_bench.exe all
```

Options: `exp1` … `exp5` instead of `all`, `--runs N` (default 7 timed runs),
`--warmup N` (default 2), `--seed S` (default 42), `--out DIR` (default
`results`), `--quick` (reduced sizes, for a smoke test). Output:

- `results/raw/expN_*_raw.csv` — every timed sample;
- `results/summary/expN_*_summary.csv` — mean, median, standard deviation,
  min and max per configuration, plus operation counters;
- `results/summary/correctness.csv` — the untimed correctness checks (the
  exit code is the number that failed);
- `results/summary/environment.txt` — compiler, build mode, runs and seed.

Charts and markdown tables for the report: `python tools/plot_results.py`
(needs Python 3 + matplotlib, a report-only tool) writes `results/charts/`
and `results/RESULTS.md`. Each experiment's inputs, timing rules and checks
are documented at the top of its `bench/Exp*.cpp` file.

## Memory-leak check

Debug builds of `smartrace_tests` and `smartrace_cli` enable the MSVC CRT
debug heap's leak report: any block still allocated at exit is printed to
stderr as `Detected memory leaks!`. A clean run prints nothing.

## Report and submission

- `report/` holds the technical report's chapters as Markdown (see
  `report/README.md` for who writes what and how to make the PDF).
- `tools/package.ps1` builds Release, runs the tests, and writes
  `build/package/24I-3071_24I-3000_<Section>.zip` (sources, data, results,
  report, and `bin/` with the Release executables). `-Verify` then rebuilds
  and re-tests from the extracted ZIP in an empty folder:

```bash
powershell -ExecutionPolicy Bypass -File tools/package.ps1 -Section BCS-5A -Verify
```

## API status (for the GUI)

`SmartRaceApi.h` is **v1.0 and frozen** (`SMARTRACE_API_VERSION 10`): from
here on, changes are additive only. Everything the five screens need is
real: circuits and validation, races, resource planning, the championship,
and the Algorithm Lab (`ApiLabRun` -> `ApiLabFrame(step)` for any step,
with pseudocode lines, counters and per-vertex/edge/cell state), including
the backtracking track generator (`LabAlgo::TrackGenerator`,
`ApiLabSaveGenerated`, `ApiLoadCircuitFile`).

## Project layout

```
src/core/    containers and utilities with no STL (DynArray, Queue, Stack,
             MinHeap, LinkedList, Pool, UnionFind, String, Rng, Timer, Log)
src/model/   Graph, Vertex/Edge, cost model, frozen game rules
src/algo/    BFS, DFS, Dijkstra, A*, DP, Greedy, Backtracking, Kruskal,
             sorting, top-k (added phase by phase, see impl_plan.md)
src/sim/     event queue, effects, race engine, opponents, championship
src/io/      track file loader/writer, CSV output
src/api/     SmartRaceApi.h/.cpp — the one interface the GUI uses
src/app/     the GUI (Rayyan's)
cli/         headless driver / API usage sample
tests/       unit + integration tests (TEST()/CHECK() framework)
bench/       smartrace_bench: the 5 required experiments
data/        rules.cfg and the .trk circuit files
tools/       gen_circuits.py -- authoring aid that generates and validates
             data/circuits/*.trk (Python; not part of the game or its build)
```

To regenerate the circuits after editing their design:

```bash
python tools/gen_circuits.py
```

## Coding rules

See `ProjectRestrictions.txt`: C/C++, no STL, no `<algorithm>`, no smart
pointers (raw pointers only), every algorithm hand-written, Allman braces.

## Acknowledgements

No third-party libraries are used by the backend. Any GUI library choice is
documented by the GUI side.
