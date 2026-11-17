# Next session prompt — final manual acceptance

Use the latest Algo-SmartRace main. Pull with --ff-only and inspect any local changes before work. Read handoff.md, docs/gui-partner-review.md and docs/gui-verification.md. API v1.1 integration and report assembly are implemented; do not repeat backend Phases 5/6 or invent another backend task.

Execute the still-manual column of the verification matrix using the real GUI: keyboard focus/text entry; physical manual road/fuel/nitro/wait controls; 1280×720 and other Windows display scales; wide refuel-table scrolling; intermediate Lab stepping/playback and generated-track save/explore. Preserve session pause/time through live Explorer/Lab round trips. Record actual evidence, fix only reproduced GUI issues, and rerun checks appropriate to those changes. Keep algorithms, circuits, measurements and partner-owned backend sources intact.

Run the GUI contract checks and rendering smoke, rebuild the report if screenshots or text change, and use tools/package.ps1 -Verify for the final archive. Report what was actually exercised separately from automated actions. Pull again before a normal push and make meaningful commits; never force-push. If everything passes, report that result instead of creating speculative work.
