# Handoff to Rayyan — 17 Nov 2026

From Wasay (24I-3071) to Rayyan (24I-3000). Deadline: **Sat 5 Dec 2026, 11:59 PM**.

The backend is finished: all phases, the experiments and the backend report chapters. What's left before submission is mostly on the GUI side, plus a few joint steps. This page lists exactly what changed since your GUI commits, what to wire up, and what we still have to do together.

---

## 1. First: how we avoid stepping on each other again

Your three GUI commits landed on top of Phase 5 and nothing was lost, but we were lucky. From now on:

1. **Before you start work:** `git pull`.
2. **Before every push:** `git pull` again, rebuild, run the checks (section 3), then `git push`.
3. **Never** `git push --force` to `main`.
4. Ownership stays as agreed. If you need something changed in the other person's area, write it down in `docs/gui-backend-handoff.md` (as you did) and tell the other person directly:
   - **You:** `src/app/`, `third_party/`, `docs/gui-*.md` and the GUI block of `CMakeLists.txt`.
   - **Me:** everything else under `src/`, plus `tests/`, `bench/`, `cli/`, `data/`, `results/`, `tools/` (except your `commit-gui.ps1`) and `report/04`–`09`.

## 2. What changed since your GUI baseline (`4fbf42d`, Phase 5)

| Commit | What | Affects the GUI? |
|---|---|---|
| `e7730b8` Phase 6 | report chapters 4–9, `tools/package.ps1`, results re-run | no code changes you need to make |
| (same) v1.0.2 | the **human autopilot now routes with A*** (same optimal cost as Dijkstra); a scripted event aimed at the wrong kind of element (e.g. a station outage on a checkpoint) is **rejected and logged** | behaviour only; the race log may show "… rejected" lines |
| v1.1 (this push) | new API functions and fields answering your handoff requests H1–H3, H7, H8 | **yes**, see section 4 |

**Your code compiles unchanged.** v1.1 only adds functions and appends fields to the end of structs, and you initialise them with `= {}`. Verified on the merged tree: Debug and Release build all six targets, `smartrace_gui_checks` passes 134/134, `smartrace_tests` passes 129/129, and `smartrace.exe` starts and runs. I did **not** edit anything in `src/app/`.

## 3. Build and check (every time before you push)

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
cmake --build build --config Release
build\Debug\smartrace_tests.exe          # expect 129/129, and no "Detected memory leaks!" on stderr
build\Debug\smartrace_gui_checks.exe     # expect 134 checks, 0 failures
```

Run both from the repository root. Note: `smartrace.exe` started fine on my machine, so if Windows Application Control blocks it on yours, do the manual checks (section 5) on my laptop or an allowed session.

## 4. Wire up API v1.1 (your requests H1–H3, H7, H8)

Full contract: the comments in `src/api/SmartRaceApi.h` (search for "v1.1") and the "Backend response" table at the end of `docs/gui-backend-handoff.md`. In short:

**H1: race-lap alternatives the player can actually pick (Explorer "Routes").**

```cpp
RouteCompareView laps = ApiCompareRaceRoutes(evalPreset);   // ordered: Start -> CP1..CP10 -> Finish
for (int i = 0; i < laps.candidates.count; ++i)
{
    const RouteCandidateView& c = laps.candidates[i];       // label, totals under evalPreset, vertexPath, edgePath
    // "Use this route":
    //   before a race: settings.defaultPreset = (Preset)c.sourcePreset; ApiSetSettings(settings);
    //   during a race: ApiCmdPreset((Preset)c.sourcePreset);
}
```

Each candidate is exactly the lap the autopilot drives under `sourcePreset` (a test checks this). Presets that choose the same roads appear once. During a race the candidates start from the human's position. Keep showing `ApiCompareRoutes` as "unrestricted exploration"; its meaning hasn't changed.

**H2: a structured route between any two vertices (Explorer / Lab endpoint search).**

```cpp
RouteView r = ApiPlanRoute(source, target, preset, RouteAlgo::AStar);   // or Dijkstra
if (!r.found) ShowError(ApiLastError());   // bad id or unreachable; blockedLegIndex == 0
// r.vertexPath, r.edgePath, totals, and new counters: r.expanded, r.relaxed, r.heapPushes, r.heapPops
```

`ApiPlanLap` now fills the same four counters, which is good for a "Dijkstra vs A*: expanded N vs M" label.

**H3: exact per-road costs and road-by-road candidate overlays.**

```cpp
ArrayView<double> costs = ApiEdgeCosts(preset);   // index = edge id, -1 = blocked; live during a race
// RouteCandidateView now has edgePath (count == vertexPath.count - 1) and sourcePreset (-1 = DFS alternative)
```

The sum of `costs` over a route's `edgePath` equals its `totalCost`. Use this for the map's cost labels instead of the raw distance. Refresh it after `ApiRaceStep`, because it's valid only until the next step.

**H7: Lab resources from the live race.** Set `m_input.useRaceState = true;` (add a toggle; the default is `false`). While a race is running, Knapsack DP, Greedy and Refuel DP then plan from the human's current vertex, fuel, nitro and remaining laps. With no race running they fall back to the pre-race baseline and say so.

**H8: structured Lab data source.**

```cpp
LabRunInfo info = ApiLabRunInfo();   // after ApiLabRun
// info.source: LoadedCircuit, LiveRace, BriefExample, BriefExampleFallback,
//              ChampionshipStandings, RandomKeys, GeneratedTrack, CircuitEvents
// info.description: one display line; info.sourceVertex / info.targetVertex: the ones actually used
```

Show a badge from `info.source` instead of parsing `ApiLabResultText()`, and highlight `sourceVertex`/`targetVertex` on the map.

Remember to update `docs/gui-backend-handoff.md` once these are wired.

## 5. Your remaining GUI work

1. **Wire v1.1** (section 4).
2. **Run the manual matrix** in `docs/gui-verification.md` (18 areas). Mark each row only after actually doing it. Please also check:
   - **Manual driving:** autopilot off, then choose roads, buy fuel at a station (the slider is pre-filled with `ApiRecommendedRefuel()`), and use nitro.
   - **Race → Lab → Back** mid-race, with Dijkstra on the live graph after the road block (C1 Harbor, 40 s).
   - **Generator seed 2:** attempt 0 is rejected for the fuel plan and attempt 1 accepted. Then Save & play the generated track.
3. **Screenshots** for the report: each of the 5 screens, plus the Lab in each view (graph, array, table, events).
4. **Release build** runs from `build\Release\` with `data\` next to it (the build copies it).

## 6. Report: your chapters

Everything is in `report/`. Read `report/README.md` first; it explains the file layout, which charts to include, the acknowledgement text and how to make the PDF.

| File | Status |
|---|---|
| `01_title.md` (title page: project, course, section, both names and roll numbers) | **you** |
| `02_introduction.md` (introduction and problem statement) | **you** |
| `03_architecture.md` (architecture and game flow: screens, navigation, the one-header API boundary; I can add a backend diagram) | **you** |
| `04`–`09` | done by me. Please read `06_integration.md` and check the "Screen" column matches your GUI. |
| `07_testing.md` | add a short "GUI testing" subsection: the 134 contract checks and the manual matrix results |
| `09_challenges_limitations.md` | add the GUI's limitations |
| `10_conclusion.md` (conclusion and acknowledgements) | joint; raylib/raygui and matplotlib acknowledgement text is in `report/README.md` |

The brief requires the final report as **one PDF**.

## 7. Packaging (joint, Fri 4 Dec)

```powershell
powershell -ExecutionPolicy Bypass -File tools/package.ps1 -Section <our section> -Verify
```

This builds Release (including `smartrace.exe`), runs the tests, and writes `build\package\24I-3071_24I-3000_<Section>.zip` with sources, `third_party/`, data, results, report and a `bin\` folder of ready-to-run executables. `-Verify` then extracts the ZIP into an empty folder, rebuilds and re-tests there. It warns if the GUI exe or the report PDF is missing. We need to fill in our actual section.

## 8. Dates

| When | What |
|---|---|
| by Tue 24 Nov | v1.1 wired, manual matrix run, screenshots taken |
| ≈ Mon 30 Nov | **joint walkthrough** (≈1 h): I explain the 12 algorithms and the race engine, you explain every screen and which API call feeds it. The brief says both of us must understand the whole project for the viva. |
| by Wed 2 Dec | your report chapters done; PDF assembled |
| Fri 4 Dec | package with `-Verify`, check the ZIP on a clean folder, one day of buffer |
| Sat 5 Dec | submit (one of us uploads to Google Classroom) |

## 9. Quick reference

- Backend contract: `src/api/SmartRaceApi.h` (changelog at the top) and `src/api/ApiTypes.h`
- Open and answered requests: `docs/gui-backend-handoff.md`
- Original screen-by-screen plan: `frontend_plan.md`
- Experiment results and charts: `results/RESULTS.md`, `results/charts/`
- Usage example of every API call: `cli/CliMain.cpp` (`build\Release\smartrace_cli.exe`)
