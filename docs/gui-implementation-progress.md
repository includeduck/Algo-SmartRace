# Project status - 21 November 2026

## Current baseline

Main includes `31af129` (Wasay's completeness corrections) after the GUI commit
`1d7c3e9`. The integrated local checkout was fast-forwarded normally. The old
outer working tree remains preserved; no backend source, authored tracks or
measured experiment CSVs were changed by this documentation/package update.

## Findings

| Finding | Current status | Remaining acceptance |
|---|---|---|
| F01 cost-change replanning | Backend corrected; regression suite passes | Physical live-driving observation |
| F02 nitro fuel enforcement | Backend corrected; affordable/held/expiry tests pass | Physical nitro control and held-state usability |
| F03 overlapping service effects | Backend corrected; overlap/collection tests pass | Physical event/service observation |
| F04 pickup feasibility | Backend corrected; individual/combined/order tests pass | Physical resource-control acceptance |
| F05 imported-track validation | Backend corrected; malformed/boundary/API gate tests pass | Physical error/retry acceptance |
| F06 current archive | Candidate verification tracked in delivery log | Final acceptance/joint review before submission |
| F07 physical GUI acceptance | Pending; no native desktop input tool available | Complete Still manual matrix in gui-verification.md |
| F08 report and PDF | Current counts/wording updated; regenerated PDF checked as logged | Incorporate actual manual outcomes, then joint review |
| F09 packaged docs | Package script includes docs; candidate contents checked as logged | Preserve passing link/membership checks after future changes |
| F10 event playback labels | Corrected and verified | No outstanding implementation change |

## Verified integration results

Debug and Release each built all targets and passed 147 backend tests / 28,554
checks and 370 GUI contract checks. Release application smoke passed 78 checks,
including a complete three-race championship. Host compiler: MSVC 19.50;
Wasay's independent environment: MSVC 19.51. Original timing measurements were
preserved; no benchmark rerun was needed for these GUI/documentation changes.

Current PDF/archive, clean-folder builds, packaged-app smoke and source/hash
checks are recorded in `gui-delivery-verification.md`. These checks do not imply
physical mouse/keyboard acceptance. Advanced challenges remain bonus; no new
bonus algorithm or hosting/deployment work is required.

## Work completed in this update

- Refresh GUI report evidence, title/footer, conclusion and reproduction text.
- Retain Wasay's backend chapters, corrected counts, diagrams and rules.
- Refresh actual application gallery from the isolated 21 November smoke.
- Update handoff, current status and verification evidence.
- Regenerate and inspect the PDF; build/check a current package candidate.
- Commit and publish the source/report changes, with actual publication outcome
  verified from Git history rather than assumed from this status document.

## Remaining user/team work

Complete physical mouse/keyboard and 100%/150% DPI acceptance, inspect manual
road/fuel/nitro/wait controls, wide tables, every playback control and generated
track save/explore/play; record actual outcomes. Then perform the joint report/
ZIP review and viva walkthrough. Repackage if any source/report changes follow.
Classroom upload has not been performed or authorized by this update.

The project has automated implementation evidence and a candidate delivery;
it is not yet fully accepted for submission.
