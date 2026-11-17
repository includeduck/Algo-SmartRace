# GUI dependencies

Vendored for offline Windows x64/MSVC builds. Retain both LICENSE files.

| Dependency | Version | Official source |
|---|---|---|
| raylib | 5.5, win64_msvc16 static archive | https://github.com/raysan5/raylib/releases/tag/5.5 |
| raygui | 4.0, single header | https://github.com/raysan5/raygui/tree/4.0 |

raygui's implementation is compiled exactly once in src/app/ui/RayguiImpl.cpp.
The GUI uses C APIs; application code does not use STL containers or algorithms.
The prebuilt raylib library targets Windows x64. Other platforms need a matching
raylib build and platform-specific GUI linkage.

SHA-256 of vendored inputs:

- raylib.lib: B4C0DC0D3999F528F298FA41EB1F72092B49AAFC43CF98A65926518EB7137C43
- raygui.h: 36491F2067A5FF18E366F950DDE2945BBCF9F75C1AB560FF9C8CA3AB6C7D0145
