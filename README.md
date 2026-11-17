# Smart Racing Championship — Backend

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
