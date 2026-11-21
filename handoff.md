# Project handoff - updated 21 November 2026

From Muhammad Wasay Tariq (24I-3071) to Syed Muhammad Rayyan Hassan (24I-3000). Section **BSE-5B** (Software Engineering). Deadline: **Sat 5 Dec 2026, 11:59 PM**.

Thanks for the v1.1 wiring, chapters 1–3 and 10, the screenshots and the combined PDF. I pulled your three commits (`69aa570`, `a6bc6c1`, `2668111`) and checked them on my machine. Everything builds and passes. This page now lists **only what is still left**, most of which is yours.

---

## 0. Current status from Rayyan - 21 November

Wasay's commit `31af129` has been pulled and independently verified on MSVC
19.50: Debug and Release each pass 147 backend tests / 28,554 checks and 370 GUI
checks; the isolated Release rendering smoke passes 78 checks, including the
three-race championship. F01-F05/F09 have backend fixes and regression coverage;
F10 has the event-label correction and forward/backward playback checks.

The report now uses the current results and neutral technical-report wording.
The latest architecture diagram and backend chapters are retained. The portable
path cleanup was already completed on 19 November. Current package/PDF command
outcomes are maintained in `docs/gui-delivery-verification.md` and current
finding status in `docs/gui-implementation-progress.md`. Earlier dated logs are
historical evidence, not current outstanding backend requests.

Physical keyboard/mouse acceptance, other display scales and joint release
review remain pending. No Classroom submission has been made. A package built
now is a verified candidate only to the extent recorded in the delivery log;
it must not be called fully accepted until those human checks are complete.

## 1. Rules (unchanged)

1. `git pull` before you start, and again before every push. Rebuild and run the checks (section 3) before pushing.
2. **Never** `git push --force` to `main`.
3. Ownership: **you** own `src/app/`, `third_party/`, `docs/gui-*.md`, the GUI block of `CMakeLists.txt`, `report/build_report.py`, `report/screenshots/` and report chapters 01, 02, 03 and the GUI sections of 07 and 09. **I** own the rest of `src/`, `tests/`, `bench/`, `cli/`, `data/`, `results/`, `tools/` and chapters 04–09. Chapter 10 and the PDF are joint.

## 2. Your findings F01–F05 and F09: fixed (20 Nov)

Thanks for the completeness review: all six findings were real. They are fixed in the backend, each with regression tests (`tests/test_completeness.cpp` and `tests/test_completeness_api.cpp`, 18 tests). I checked that every engine test fails when its fix is reverted. **No API change**: `SMARTRACE_API_VERSION` stays 11 and your code compiles unchanged. The full answer, including the two rule decisions you asked for, is at the end of `docs/gui-backend-handoff.md`.

| # | Fix | What you may see in the GUI |
|---|---|---|
| F01 | The human autopilot and Opponent 3 replan at their next vertex after **any** graph change (oil, speed limit, reopened road, outage), not only after a closure. Opponents 1 and 2 keep their fixed preference. | the autopilot now avoids a new oil spill ahead, and takes a reopened road |
| F02 | **Decision:** nitro is *held* while boosting would eat the fuel reserved for the rest of the current road: no boost, no extra fuel, and its 4 s don't run down. It resumes when affordable. | `RacerView.nitroActive` stays true while held; the log says "… nitro held: not enough fuel to boost on this road" |
| F03 | **Decision:** outages are counted per station (it reopens when the last ends); each booster has its own expiry; collecting takes the soonest-expiring booster (or the pickup's own charge) and leaves others lit. | stations and pickups stay in the right state with overlapping events |
| F04 | Pickup detours are offered only if driveable from the actual fuel (topping up at stations on the route) and with checkpoint order intact; a DP/greedy chosen set is re-checked as a whole and repaired. | fewer pickups may be offered when fuel is low; DP and greedy lists can lose an item |
| F05 | The loader rejects malformed, non-finite, negative or out-of-range edge values (difficulty 1–5, flags 0/1) with the line and field. `ApiRaceStart` returns `Infeasible` with "circuit failed validation: …" for a circuit that loads but fails the validator. | a bad custom file now fails at load; an invalid one can be explored but not raced |
| F09 | `tools/package.ps1` now packages `docs/`. | — |

Verified on my machine (MSVC 19.51, x64), Debug and Release:

| Check | Result |
|---|---|
| Build, all targets | pass, no warnings |
| `smartrace_tests` | **147/147, 28,554 checks**, no leak report |
| `smartrace_gui_checks` | 370/370 |
| `smartrace_cli` championship on C1–C3 | completes |
| `smartrace_gui_smoke` | **not run by me** (it rewrites `report/screenshots/`); please run it from your isolated folder |
| Benchmarks | not re-run: no experiment uses the changed code |

Report updated for the fixes: chapter 4 (nitro fuel rule, overlapping events, track-file checks), 5.6a (feasible pickups), 6 (replanning), 7 (counts, new test group, the five bugs) and 9 (limitations). PDF regenerated. Your 7.5 table still shows your 19 Nov counts (129 tests); please refresh it after your next run.

## 3. Build and check (before every push)

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
cmake --build build --config Release
build\Debug\smartrace_tests.exe          # expect 147/147, no "Detected memory leaks!"
build\Debug\smartrace_gui_checks.exe     # expect 370 checks, 0 failures
```

## 4. What you still need to do

### 4.1 Manual acceptance (the biggest open item)

Your own `docs/gui-verification.md` says the "Still manual" column has not been done. No physical mouse/keyboard acceptance outcomes have been recorded yet, and the viva will exercise those controls. Do each row, then mark it in `docs/gui-verification.md` with what you actually saw:

| Area | Do this by hand |
|---|---|
| Data loading | Use an isolated game copy with missing data, restore only that copy, then click Retry |
| Routes | Click and Shift-click endpoints; open every candidate; "Use this route" before and during a race |
| Race | Autopilot off: choose roads, wait at the C1 Harbor closure (40 s) until it reopens, buy fuel at a station, use nitro |
| Resources | Scroll the wide refuelling DP table; check every budget |
| Championship | Every sort control (Merge, Quick, Heap, top-k) |
| Lab | Every playback speed, drag-seeking, generator seed 2 → Save → explore and play the saved track |
| Layout | 1280×720, and 100% and 150% Windows display scaling if you can |
| Accessibility | Tab / Shift-Tab order, text entry in every field |

Also run the Release build from `build\Release\` and do one complete 3-race championship there. If anything breaks, fix it in `src/app/`, or write it in `docs/gui-backend-handoff.md` if it's a backend issue, and tell me.

### 4.2 Documentation and PDF status

The GUI testing section now records 147/28,554 backend checks, 370 GUI checks and
78 rendering checks, with MSVC 19.50 distinguished from Wasay's 19.51 results.
The title/footer/README use the 21 November technical-report revision. The
conclusion reports measured outcomes instead of an internal review instruction.
Physical acceptance remains explicitly unverified; replace that status only
when actual observations are supplied. See `docs/gui-delivery-verification.md`
for the PDF regeneration, visual review and package evidence.

### 4.3 Portable documentation

Completed: machine-specific username paths were removed from the partner-review
document, and `build/partner-review` is identified as local historical context,
not an evaluator requirement. The package includes linked GUI documentation.

### 4.4 Package and final release gates

Use the current candidate and evidence in `docs/gui-delivery-verification.md`.
Reproduce `tools/package.ps1 -Verify` from a short checkout path, followed by:

```powershell
python src/app/checks/CheckSubmission.py build/package/24I-3071_24I-3000_BSE-5B.zip --source-root .
```

Before submission: complete 4.1, incorporate observed GUI/report changes,
regenerate the PDF and candidate if source changes, jointly review the final
ZIP/PDF and complete the viva walkthrough. Upload to Classroom only when one
member explicitly performs/authorizes submission; packaging is not submission.

## 5. What I still do

- Review your final report wording (4.2) and the final ZIP with you on 4 Dec.
- Answer any backend issue you find in 4.1. Nothing is open now: all of H1–H8 are closed.
- The joint walkthrough for the viva (section 6).

## 6. Dates

| When | What |
|---|---|
| by Tue 24 Nov | 4.1 manual matrix done and recorded; 4.3 cleanup |
| ≈ Mon 30 Nov | **joint walkthrough** (≈1 h): I explain the 12 algorithms and the race engine, you explain every screen and which API call feeds it. Both of us must understand the whole project for the viva. |
| by Wed 2 Dec | 4.2 report wording final; PDF regenerated and committed |
| Fri 4 Dec | 4.4 final package with `-Verify` from a short path; open-the-ZIP check; one day of buffer |
| Sat 5 Dec | submit `24I-3071_24I-3000_BSE-5B.zip` (one of us uploads to Google Classroom) |

## 7. Quick reference

- Backend contract: `src/api/SmartRaceApi.h` (changelog at the top) and `src/api/ApiTypes.h`
- Requests and answers: `docs/gui-backend-handoff.md`
- Manual matrix: `docs/gui-verification.md`
- Experiment results and charts: `results/RESULTS.md`, `results/charts/`
- Usage example of every API call: `cli/CliMain.cpp` (`build\Release\smartrace_cli.exe`)
