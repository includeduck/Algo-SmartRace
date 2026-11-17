# Handoff to Rayyan — updated 17 Nov 2026 (evening)

From Muhammad Wasay Tariq (24I-3071) to Syed Muhammad Rayyan Hassan (24I-3000). Section **BSE-5B** (Software Engineering). Deadline: **Sat 5 Dec 2026, 11:59 PM**.

Thanks for the v1.1 wiring, chapters 1–3 and 10, the screenshots and the combined PDF. I pulled your three commits (`69aa570`, `a6bc6c1`, `2668111`) and checked them on my machine. Everything builds and passes. This page now lists **only what is still left**, most of which is yours.

---

## 1. Rules (unchanged)

1. `git pull` before you start, and again before every push. Rebuild and run the checks (section 3) before pushing.
2. **Never** `git push --force` to `main`.
3. Ownership: **you** own `src/app/`, `third_party/`, `docs/gui-*.md`, the GUI block of `CMakeLists.txt`, `report/build_report.py`, `report/screenshots/` and report chapters 01, 02, 03 and the GUI sections of 07 and 09. **I** own the rest of `src/`, `tests/`, `bench/`, `cli/`, `data/`, `results/`, `tools/` and chapters 04–09. Chapter 10 and the PDF are joint.

## 2. What I verified and changed since your push

| Check | Result on my machine (MSVC 19.51, x64) |
|---|---|
| Debug and Release build, all 7 targets (incl. `smartrace_gui_smoke`) | pass |
| `smartrace_tests` Debug and Release | 129/129, 28,480 checks, no leak report |
| `smartrace_gui_checks` Debug and Release | 238/238 |
| Your new GUI code against the coding rules (no STL, no smart pointers, Allman, braced bodies) | clean |
| `smartrace_gui_smoke` | **not run by me** (it rewrites `report/screenshots/`) |

My commit `1c45a74`: chapter 7 sections 7.1–7.3 had stale numbers (124 tests / 28,348 checks). They now say 129 / 28,480, so I removed your sentence in 7.5 that explained "the earlier 124-test table". I regenerated the PDF with your `build_report.py` (still 48 pages). **Pull before you touch the report.**

## 3. Build and check (before every push)

```powershell
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
cmake --build build --config Release
build\Debug\smartrace_tests.exe          # expect 129/129, no "Detected memory leaks!"
build\Debug\smartrace_gui_checks.exe     # expect 238 checks, 0 failures
```

## 4. What you still need to do

### 4.1 Manual acceptance (the biggest open item)

Your own `docs/gui-verification.md` says the "Still manual" column has not been done. Nobody has driven the game with a real mouse and keyboard yet, and the viva will. Do each row, then mark it in `docs/gui-verification.md` with what you actually saw:

| Area | Do this by hand |
|---|---|
| Data loading | Rename `data/` temporarily, start the game, press Retry after restoring it |
| Routes | Click and Shift-click endpoints; open every candidate; "Use this route" before and during a race |
| Race | Autopilot off: choose roads, wait at the C1 Harbor closure (40 s) until it reopens, buy fuel at a station, use nitro |
| Resources | Scroll the wide refuelling DP table; check every budget |
| Championship | Every sort control (Merge, Quick, Heap, top-k) |
| Lab | Every playback speed, drag-seeking, generator seed 2 → Save → explore and play the saved track |
| Layout | 1280×720, and 100% and 150% Windows display scaling if you can |
| Accessibility | Tab / Shift-Tab order, text entry in every field |

Also run the Release build from `build\Release\` and do one complete 3-race championship there. If anything breaks, fix it in `src/app/`, or write it in `docs/gui-backend-handoff.md` if it's a backend issue, and tell me.

### 4.2 Report text to finalise (your sections)

Once 4.1 is done, the report must stop saying the manual pass is pending:

1. **`report/build_report.py` line 79:** the page footer says "Review edition, 17 November 2026". Change it to the final wording (e.g. "Final report, December 2026").
2. **`01_title.md` line 17:** "This review edition combines …". Replace it with final wording, or delete it.
3. **`07_testing.md` 7.5:** replace "No manual usability pass is claimed" and the pending list with the actual manual results. Also, 7.5 says MSVC **19.50** while my chapters say 19.51. Use whichever your machine reports, but say both if they differ.
4. **`07_testing.md` 7.6** says "the unchanged partner packaging script". Write "the packaging script (`tools/package.ps1`)" instead. The examiner doesn't know who "the partner" is. Re-run 7.6 after the final package (section 4.4) and update it.
5. **`09_challenges_limitations.md` 9.4:** the last bullet says manual acceptance is pending. Update it.
6. **`10_conclusion.md`:** "Both members should review the remaining manual matrix before submission." That's a note to us, not a conclusion. Replace it with the outcome.
7. **`report/README.md`:** retitle the "Combined review edition" section (or drop it).
8. Then run `python report/build_report.py` and commit the PDF.

### 4.3 Small cleanup

- **`docs/gui-partner-review.md` line 32** contains your Windows user folder path (`C:/Users/<you>/AppData/...`). Replace it with something generic like "a short path such as `C:\sr`". That file goes into the ZIP.
- The same file refers to `build/partner-review` as "the current clean integration checkout". It's a local folder that won't exist for anyone else, so say so or drop the sentence.

### 4.4 Final package (joint, Fri 4 Dec)

The ZIP you built on 17 Nov is **out of date**, because the report and docs changed afterwards. Rebuild it at the end, after the final PDF is committed:

```powershell
powershell -ExecutionPolicy Bypass -File tools/package.ps1 -Verify      # section defaults to BSE-5B
```

You found that the clean-folder rebuild fails from a deeply nested path. Run it from a short checkout path (e.g. clone into `C:\sr`). Then open the ZIP once by hand and check that `bin\smartrace.exe` starts with `bin\data\` next to it, and that the PDF opens.

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
