# Delivery verification - 21 November 2026

## Source and acceptance boundary

Source baseline: main `31af129` plus the report/status update that contains this
record. Backend implementation, authored circuits and measured experiment CSVs
are unchanged. The candidate includes updated GUI evidence and the current PDF.
Physical keyboard/mouse acceptance, additional display scales, joint release
review and Classroom submission remain pending. No universal completion claim
is made by a passing build or archive CRC.

## Observed results

| Check | Observed outcome |
|---|---|
| Integrated Debug build/tests | Passed: 147 tests, 28,554 checks; GUI 370 checks; zero failures |
| Integrated Release build/tests | Passed: 147 tests, 28,554 checks; GUI 370 checks; zero failures |
| Isolated Release rendering smoke | Passed: 78 checks; actual App completes three-race championship |
| Short-path tools/package.ps1 -Verify | Passed: Release build/tests, ZIP extraction, independent Release rebuild/tests; 147/28,554 in each suite |
| CheckSubmission.py with --source-root | Passed: zero failures; CRC, required docs/dependencies/raw CSVs/PDF, document links, reviewed bytes and GUI binary correspondence |
| Packaged CLI from extracted bin/ with adjacent data | Exit 0; championship and Lab/generator/validator driver completes |
| Renderer smoke using extracted bin/data | Exit 0; 78 checks; full championship; no physical input claim |
| Actual packaged smartrace.exe launch | Remained running for three seconds; only the launched test process was then stopped |
| Combined PDF | 50 pages, 27 embedded images; all pages rendered/reviewed in contact sheets; corrected figure-caption and final-reference pages checked at page scale |
| PDF structural checks | No out-of-page text spans or replacement glyphs; current evidence/footer present; old review footer absent |
| Git whitespace | Passed |

Host compiler: MSVC 19.50.35730, x64, SDK 10.0.26100. Wasay's independent
reported environment uses MSVC 19.51. No new benchmark timings were generated.
The report's architecture figure retains its authored 17 November counts;
its caption explicitly directs readers to Section 7.5 for current counts.

The first PDF review found an orphan reference on page 51. Joining the wrapped
reference entries removed that spill; the resulting report has 50 pages. An
initial runtime-smoke command used an incorrect relative executable path and
failed to launch; the corrected absolute-path command passed all 78 checks.

## Reproduction and artifacts

From a current source checkout at a short Windows path:

```powershell
python report/build_report.py
powershell -File tools/package.ps1 -Verify
python src/app/checks/CheckSubmission.py build/package/24I-3071_24I-3000_BSE-5B.zip --source-root .
```

This host needs Visual Studio's CMake directory on PATH, normal PATHEXT, and
MSBuild properties TrackFileAccess=false / PreferredToolArchitecture=x64.
The short D:/workspace location was not writable, so this run used an isolated
snapshot under the allowed Windows temporary folder. Final logs and artifact
hashes are copied to the integration checkout's ignored `build/delivery-20261121/`.
The original user working tree and authored data were never renamed for testing.

Current candidate: `build/package/24I-3071_24I-3000_BSE-5B.zip`.
Combined PDF: `report/24I-3071_24I-3000_BSE-5B_Report.pdf`.
The manifest beside the verification logs identifies exact ZIP/PDF SHA-256
values and the source revision. It is external to the ZIP to avoid a circular
self-hash. The ZIP is a release candidate pending the human checks above.

Windows Defender PowerShell commands were unavailable. A non-remediating custom
ZIP scan was attempted with the installed MpCmdRun.exe; it exited 2 with HRESULT
0x80508023 before successful completion. No malware-free certification or
completed antivirus pass is claimed. Complete the assignment's antivirus check
in the normal developer session before submission, along with the joint PDF/ZIP
review. The scan log is retained with the delivery evidence.
