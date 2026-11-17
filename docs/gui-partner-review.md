# Partner integration review — 17 November 2026

Reviewed main at 16c0304a83a85da54ae74c48fb29764093dfdcd2 after a clean clone and fast-forward pull. Partner Phase 5 and Phase 6 are present: experiments, committed raw/summary measurements, charts, backend report chapters, tests and packaging. API v1.1 (macro 11) answers the handoff requests. No backend algorithm, race rule, circuit, measurement or existing test was changed by this GUI integration.

## Delta and completed work

| Contract | Previous GUI / API | Partner addition | GUI call site and completed action |
|---|---|---|---|
| H1 | Unrestricted alternatives could skip checkpoints | ApiCompareRaceRoutes plus sourcePreset | ExplorerScreen Refresh/Update: separate ordered race candidates, exact overlays, pre-race defaultPreset or live ApiCmdPreset |
| H2 | Only lap queries returned structured metrics | ApiPlanRoute plus operation counters | Explorer selected endpoint mode; Lab resolved endpoint summary and source/target markers |
| H3 | Road scalar and candidate road identity unavailable | ApiEdgeCosts and candidate.edgePath | MapView, Explorer and Race: actual optimized scalar, blocked labels, exact road overlay; narrow maps use selected-road details |
| H4 | Starting nitro setting previously ignored | Engine now honours 0–3 | Settings screen exposes the existing validated setting |
| H5 | Settings validation/documentation incomplete | Range validation; ApiInit still only sets directory | Retain actual circuit-load validation and persistent recovery errors |
| H6 | Experimental/report work pending | Phase 5/6 measurements, charts, report 04–09, package tool | Preserve measurements and backend chapters; complete GUI chapters, screenshots and combined PDF; verify submission archive |
| H7 | Resource Lab was pre-race baseline | LabInput.useRaceState | Lab live/baseline toggle; enters live from an active race; source badge describes actual data |
| H8 | Example fallback could only be inferred from prose | ApiLabRunInfo / LabDataSource | Explicit enum-based source badges, copied description and resolved endpoints |
| Seed semantics | UI implied random events | eventSeed is reserved | Settings now says authored scripts; seed does not claim to randomize events |
| Display | No demonstrated resized DPI acceptance | Application rendering check added here | WindowCanvas/Theme align actual Windows client pixels, projection, mouse scaling and scissor clips; 800×600 capture inspected |

The GUI uses only SmartRaceApi.h for backend operations. Borrowed route arrays needed after another route call are copied. Scalars retained in Lab deliberately drop borrowed route paths. Live queries and cost labels are refreshed before drawing; navigation stops race stepping and returning retains the same session.

## Verification and remaining work

See gui-verification.md for executed commands, counts, screenshots and the remaining manual matrix. All 13 Lab modules and a complete three-race championship run through the real application update loop. Harbor is advanced past 40 seconds before Race → Explorer/Lab → Back checks. Seed 2 rejects attempt 0 and accepts attempt 1 in the real generator.

No backend request remains open from H1–H8. Manual keyboard/mouse driving, focus ergonomics, additional display scales and exhaustive screen interactions still need human acceptance; automated actions do not establish those. The five benchmark studies use the partner's committed measurements, not newly invented timing data.

The original root checkout and its local planning files are preserved. The current clean integration checkout is build/partner-review; it has independent writable Git metadata. Further work should use that checkout or a fresh pull rather than mixing the old root GUI with the new backend.

## Submission archive evidence

The unchanged tools/package.ps1 -Verify completed successfully after extracting the sources into C:/Users/rayya/AppData/Local/Temp/smartrace-final-20261117. It built every Release target, ran 129 tests / 28,480 checks, created the ZIP, then extracted that ZIP into a new folder, rebuilt and passed the same tests again. The original deep checkout path failed CMake compiler discovery during the clean extraction; use a short path when reproducing packaging on Windows. The host also needed CMake on PATH, PATHEXT restored and TrackFileAccess=false / PreferredToolArchitecture=x64 to avoid its file-tracker issue. The partner's packaging script was not edited.

The resulting archive is build/package/24I-3071_24I-3000_BSE-5B.zip. It includes the native executable with adjacent data/, source, pinned dependencies, raw/summary results, charts, report sources and combined PDF. Documentation/report updates after the first clean verification do not change game source; the final archive is CRC checked after regeneration.
