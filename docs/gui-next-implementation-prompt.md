# Next session - physical acceptance and joint release review

Read `handoff.md`, `docs/gui-verification.md`, `docs/gui-implementation-progress.md`
and `docs/gui-delivery-verification.md`. Pull normally and preserve local work.
F01-F05/F09 were fixed by Wasay in 31af129; F10 is corrected in the GUI. Do not
repeat those implementations or overwrite measured experiments/backend chapters.

The remaining acceptance work requires actual keyboard/mouse interaction:

1. Launch the Release game with adjacent data. Use an isolated copy for missing-
   data Retry; never rename or delete the authored source data as a test.
2. Click/Shift-click route endpoints and inspect/apply every candidate before
   and during a race. Verify pause/time retention through Explorer/Lab returns.
3. Turn autopilot off and choose roads, wait through the Harbor closure/reopen,
   buy/skip fuel, activate nitro and observe held-nitro behavior in the log.
4. Navigate wide DP tables, representative zero/max budgets, unavailable service
   and infeasible cases. Exercise each sort/top-k/history control.
5. Exercise every Lab playback rate, step/back/drag seek and seed-2 generator
   Save -> explore -> play. Confirm session locks preserve active races.
6. Check keyboard focus/text input, 1280x720 and 100%/150% Windows display scales.

Record observations by row in gui-verification.md. If no native input tool is
available, ask the user to perform these and report results; do not count smoke
screenshots or Ui::PostAction as physical acceptance. Fix only reproduced GUI
issues; route new backend issues through the handoff unless separately authorized.

After actual acceptance, update the report with those results, regenerate/review
the combined PDF, run tools/package.ps1 -Verify from a short path, and run the
read-only contents checker with --source-root. Rebuild/recheck affected code,
commit, synchronize normally and push without force. Jointly review the final
PDF/ZIP and complete the viva walkthrough. Classroom submission requires explicit
authorization; a candidate build does not establish that it was submitted.
