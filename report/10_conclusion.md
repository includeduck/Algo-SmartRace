# 10. Conclusion and References

Smart Racing Championship connects graph traversal, weighted routing,
resource optimization, deterministic event heaps, track generation and sorting
within one playable application. The public API makes the ownership boundary
clear: the backend computes domain results, and the GUI presents them and sends
commands. The Lab's traces come from the gameplay implementations.

The five experiments separate measured execution time from theoretical bounds
and include operation counters and correctness checks. The GUI makes those
roles observable through routes, resources, changing roads, traces and rankings.
API v1.1 also removes the earlier need for guessed road overlays or prose-based
identification of Lab inputs.

Automated checks and real rendered screenshots support the current review.
They do not replace final manual driving and accessibility acceptance. The
submission package has been extracted, rebuilt and tested successfully from a
short Windows path. Both members should review the remaining manual matrix
before submission.

## 10.1 Acknowledgements

- raylib 5.5, by Ramon Santamaria and contributors, supplies windowing, rendering
  and its bundled font. The pinned Windows x64 library and zlib/libpng license
  are retained under third_party/raylib.
- raygui 4.0, by Ramon Santamaria and contributors, supplies immediate-mode
  widgets. Its original header and zlib/libpng license are retained under
  third_party/raygui.
- Python 3 and matplotlib are used for report-only experiment charts and
  circuit-authoring utilities; they are not part of the executable game.
- ReportLab is used only to assemble this PDF from the report Markdown and
  existing charts/screenshots. It is not linked into the game.
- The backend's algorithms and supporting containers are implemented in the
  project source. GUI rendering libraries are not substitutes for those algorithms.

## 10.2 References

1. Smart Racing Championship project assignment, Design and Analysis of
   Algorithms, Sections 1-9; deadline 5 December 2026.
2. raylib official source and release: https://github.com/raysan5/raylib/tree/5.5
3. raygui official source: https://github.com/raysan5/raygui/tree/4.0
4. matplotlib documentation: https://matplotlib.org/stable/
5. ReportLab project: https://www.reportlab.com/
6. Reproducibility evidence: results/raw/, results/summary/,
   results/hardware.txt and results/RESULTS.md in this repository.
7. Implementation and verification: src/api/SmartRaceApi.h,
   docs/gui-verification.md, tests/ and src/app/checks/.
