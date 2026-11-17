# Smart Racing Championship — Frontend (GUI) Implementation Plan

**For:** Rayyan (24I-3000), GUI owner   **Backend:** Muhammad Wasay Tariq (24I-3071)
**Written:** Tue 17 Nov 2026   **Deadline:** Sat 05 Dec 2026, 11:59 PM
**Backend status:** Phases 0–4 done and on `main`. The API (`src/api/SmartRaceApi.h` + `ApiTypes.h`) is **v1.0 and frozen**: every call below exists and works today.

Follow this document top to bottom. Each section says exactly which files to create, which API calls to make, and how to check it works. If something here disagrees with the comments in `SmartRaceApi.h`, the header wins. Tell Wasay and he will fix this document.

---

## 0. Read this first

### 0.1 What you own and what you don't

| You (GUI) | Wasay (backend) |
|---|---|
| Everything under `src/app/` and `third_party/` | `src/core`, `model`, `algo`, `sim`, `io`, `api`, `cli/`, `tests/`, `bench/`, `data/` |
| The `smartrace` executable target in `CMakeLists.txt` | All other targets |
| Drawing, input, layout, screen flow | Every number the screens show |

**Rule 1: the GUI includes exactly one backend header:** `#include "api/SmartRaceApi.h"` (it pulls in `ApiTypes.h`). Never include anything from `src/algo`, `src/sim`, `src/model`, `src/core` or `src/io`.

**Rule 2: the GUI never runs algorithm logic.** It never searches the graph, plans a route, sorts standings or computes a cost. If a screen needs a number, call the API. The only "logic" you may write is drawing helpers: fit-to-screen, hit-testing, finding the edge between two vertices so you can draw a path, and filtering a list for display. Examiners will look for this split; it is how we prove "the Lab uses the same code as gameplay".

**Rule 3: the course coding rules apply to your code too** (`ProjectRestrictions.txt`):
- C/C++ only. **No STL**: no `std::string`, `std::vector`, `<iostream>`, `<algorithm>`, `<memory>`, `<functional>`, `<chrono>`, and so on. Use fixed-size arrays, `char buf[N]` + `snprintf`, and raw `new`/`delete`.
- **No smart pointers.** Whoever `new`s an object `delete`s it.
- **Allman braces.** `{` goes on its own line for functions, classes, `if/for/while/switch`. A single-statement `if`/`while` is either on **one line** (`if (x) Foo();`) or braced on separate lines. **Never** put an unbraced body on the next line. Treat `else` the same way: use braces.

```cpp
// CORRECT
void DrawHud()
{
    if (paused) DrawText("PAUSED", 20, 20, 30, RED);
    if (fuel < 100)
    {
        DrawLowFuelWarning();
    }
    else
    {
        DrawFuelBar();
    }
}

// WRONG
void DrawHud() {            // brace on same line
    if (paused)
        DrawText(...);      // unbraced body on next line
}
```

C headers are fine: `<cstdio>` (`snprintf`), `<cmath>` (`sqrtf`, `cosf`), `<cstdint>`.

### 0.2 GUI library: raylib 5.5 + raygui 4.0

Use **raylib** (windowing, drawing, input) and **raygui** (buttons, sliders, lists). Both are plain C, so the no-STL rule holds. Both must be acknowledged in the README and the report (section 13).

### 0.3 How the backend talks to you

| Topic | Rule |
|---|---|
| Threading | Single thread. Call the API only from your main loop. The backend never calls you back. |
| Data | Plain structs and `ArrayView<T>` = `{ const T* data; int count; }`, indexable with `[]`. |
| **Lifetime** | Every pointer or view you get back is **borrowed**. It stays valid only until the next call named in that function's comment in `SmartRaceApi.h` (for example, `ApiRaceSnapshot()` is valid until the next `ApiRaceStep`). **Never store a view across frames.** Call the API again next frame, or copy the values into your own arrays (strings: `snprintf` into your own `char[]`). |
| Errors | Calls that can fail return `ApiStatus`. On anything other than `ApiStatus::Ok`, show `ApiLastError()` in a toast (section 3.4). No call crashes on bad input. |
| Time | Race time moves **only** when you call `ApiRaceStep(ticks)`. One tick = 20 ms of race time. Pausing is simply not calling it. |
| Units | `fuelTenths`: 0..1000 = 0.0..100.0 units. `*TenthsSec`: tenths of a second. `*Ms`: milliseconds. `INT_MAX` (`0x7fffffff`) in a lap time means "no lap yet". `-1` in `finishTimeMs` means "did not finish". |

The best usage example is `cli/CliMain.cpp`. It calls almost everything below and prints the results. Run `build/Debug/smartrace_cli.exe` once to see what the data looks like.

---

## 1. Setup (day 1)

### 1.1 Toolchain

Use the same toolchain as the backend, or the `.lib` will not link:
- Visual Studio 2026 (MSVC 14.5x), workload "Desktop development with C++", **x64**.
- CMake ≥ 3.20.

Build the backend once to check your machine:

```bash
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
```

```bash
cmake --build build --config Debug
```

```bash
build/Debug/smartrace_tests.exe
```

All tests must pass. Run commands from the repository root, because they read `data/`.

### 1.2 Get raylib and raygui (vendored, so the ZIP builds offline)

1. From the raylib GitHub releases page, tag **5.5**, download **`raylib-5.5_win64_msvc16.zip`**. Unzip it and copy:
   - `include/raylib.h`, `include/raymath.h`, `include/rlgl.h` → `third_party/raylib/include/`
   - `lib/raylib.lib` → `third_party/raylib/lib/`
2. From the raygui GitHub releases page, tag **4.0**, take **`src/raygui.h`** → `third_party/raygui/raygui.h`.
3. `.gitignore` ignores `*.lib`. Add this line at the end of `.gitignore` so the library gets committed:
   ```
   !third_party/raylib/lib/raylib.lib
   ```
4. Copy the raylib and raygui licence files (both zlib/libpng) to `third_party/raylib/LICENSE` and `third_party/raygui/LICENSE`.

### 1.3 CMake target

Append this block to the **end** of the root `CMakeLists.txt`. Don't change the backend targets above it.

```cmake
# ---------------------------------------------------------------------------
# smartrace — the GUI (owned by Rayyan, 24I-3000). raylib 5.5 + raygui 4.0,
# vendored under third_party/. Links the backend only through smartrace_core.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SMARTRACE_APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/app/*.cpp"
)
if(SMARTRACE_APP_SOURCES)
    add_executable(smartrace ${SMARTRACE_APP_SOURCES})
    target_include_directories(smartrace PRIVATE
        "${CMAKE_SOURCE_DIR}/src/app"
        "${CMAKE_SOURCE_DIR}/third_party/raylib/include"
        "${CMAKE_SOURCE_DIR}/third_party/raygui"
    )
    target_link_libraries(smartrace PRIVATE
        smartrace_core
        "${CMAKE_SOURCE_DIR}/third_party/raylib/lib/raylib.lib"
        winmm gdi32 opengl32 user32 shell32
    )
    if(MSVC)
        # raylib.lib is built against the release CRT; silence the Debug clash.
        target_link_options(smartrace PRIVATE $<$<CONFIG:Debug>:/NODEFAULTLIB:msvcrt.lib>)
        # No console window in Release; keep it in Debug for printf debugging.
        target_link_options(smartrace PRIVATE $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>)
        # F5 in Visual Studio runs with the exe's folder as working directory.
        set_target_properties(smartrace PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:smartrace>")
    endif()
    # The game reads data/ relative to its working directory: copy it next to the exe.
    add_custom_command(TARGET smartrace POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:smartrace>/data")
endif()
```

Run it from `build/Debug/` (or `build/Release/`) so `data/` is found next to the exe:

```bash
cd build/Debug && ./smartrace.exe
```

### 1.4 Day-1 smoke test

Create `src/app/main.cpp` with just enough code to prove the link works:

```cpp
#include "raylib.h"
#include "api/SmartRaceApi.h"

#include <cstdio>

int main()
{
    InitWindow(1280, 720, "Smart Racing Championship");
    SetTargetFPS(60);

    ApiStatus status = ApiInit("data");
    char line[128];
    snprintf(line, sizeof(line), "ApiInit: %s, circuits: %d, API v%d",
        status == ApiStatus::Ok ? "Ok" : ApiLastError(), ApiCircuitCount(), SMARTRACE_API_VERSION);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText(line, 40, 40, 24, DARKGRAY);
        EndDrawing();
    }

    ApiShutdown();
    CloseWindow();
    return 0;
}
```

**Done when:** the window shows `ApiInit: Ok, circuits: 3, API v10`.

**Known gotchas:**
- **Never include `<windows.h>`** in GUI code. It clashes with raylib (`Rectangle`, `CloseWindow`, `DrawText`, `ShowCursor`, ...). The backend already keeps it out of its headers.
- If you see `LNK4098 defaultlib 'MSVCRT' conflicts`, the `/NODEFAULTLIB:msvcrt.lib` line above handles it for Debug.
- raygui needs its implementation compiled **once**: see `ui/RayguiImpl.cpp` in section 2.

---

## 2. Code structure

Create exactly these files under `src/app/`:

```
src/app/
├─ main.cpp                    window, main loop, App lifetime
├─ App.h / App.cpp              owns the screens, switches between them, holds AppState
├─ AppState.h                   shared state that crosses screens (mode, names, paths)
├─ Screen.h                     abstract base class for every screen
├─ screens/
│  ├─ MainMenuScreen.h/.cpp
│  ├─ InstructionsScreen.h/.cpp
│  ├─ SettingsScreen.h/.cpp
│  ├─ CircuitSelectScreen.h/.cpp
│  ├─ ExplorerScreen.h/.cpp        Screen 2: Track Explorer / Route Planner
│  ├─ RaceScreen.h/.cpp            Screen 3: Racing
│  ├─ ResultsScreen.h/.cpp         race results, after every race
│  ├─ ChampionshipScreen.h/.cpp    Screen 5: Championship Dashboard
│  └─ LabScreen.h/.cpp             Screen 4: Algorithm Lab
└─ ui/
   ├─ RayguiImpl.cpp            #define RAYGUI_IMPLEMENTATION + #include "raygui.h" (nothing else)
   ├─ Theme.h/.cpp              colours, font sizes, raygui style setup
   ├─ MapView.h/.cpp            draws a GraphView with pan/zoom, highlighting and hit-testing
   ├─ Format.h/.cpp             ms → "1:23.456", tenths → "12.3", enum → text
   └─ Toast.h/.cpp              short error/info messages that fade out
```

Every other `.cpp` that uses raygui does `#include "raygui.h"` **without** the define.

### 2.1 Screen base class and screen ids

```cpp
// Screen.h
#pragma once

enum class ScreenId
{
    MainMenu,
    Instructions,
    Settings,
    CircuitSelect,
    Explorer,
    Race,
    Results,
    Championship,
    Lab,
};

class App;

class Screen
{
public:
    virtual ~Screen()
    {
    }

    virtual void Enter(App* app) = 0;            // called every time the screen becomes active
    virtual void Update(App* app, float dt) = 0; // input + API calls
    virtual void Draw(App* app) = 0;             // drawing only, no API calls that change state
};
```

### 2.2 AppState (data that crosses screens)

```cpp
// AppState.h
#pragma once

#include "api/SmartRaceApi.h"

enum class GameMode
{
    None,
    QuickRace,
    Championship,
};

struct AppState
{
    GameMode mode;                 // what "Start race" / "Continue" mean
    int selectedCircuit;           // 0..2, or -1 when the generated track is loaded
    bool generatedTrackSaved;      // a generated .trk exists at kGeneratedTrackPath
    bool labOpenedFromRace;        // the Lab must not load another circuit (it would end the race)
    ScreenId labReturnScreen;      // where the Lab's "Back" goes
    char racerNames[10][32];       // copied from the snapshot at race start (results rows have only ids)
    int racerCount;
};

const char* const kGeneratedTrackPath = "generated_track.trk";
```

### 2.3 App

`App` owns one instance of each screen (`new` in `Init`, `delete` in `Shutdown`), stores `Screen* m_screens[9]`, the current `ScreenId`, and an `AppState`. It has `void Go(ScreenId id)`, which sets the current screen and calls `Enter`. It also has `void RequestQuit()`.

`main.cpp`:

```cpp
int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1600, 900, "Smart Racing Championship");
    SetTargetFPS(60);
    Theme::Apply();

    App* app = new App();
    app->Init();                      // ApiInit("data"); on failure shows the error on the menu
    while (!WindowShouldClose() && !app->QuitRequested())
    {
        app->Update(GetFrameTime());
        BeginDrawing();
        ClearBackground(Theme::kBackground);
        app->Draw();
        Toast::Draw();
        EndDrawing();
    }
    app->Shutdown();                  // ApiShutdown()
    delete app;
    CloseWindow();
    return 0;
}
```

---

## 3. Shared UI pieces (build these before the screens)

### 3.1 Theme (`ui/Theme.h`)

One place for every colour, so the legend is consistent on all screens. Use these colours:

| Thing | Colour | Notes |
|---|---|---|
| Background | `{ 24, 26, 32, 255 }` | dark |
| Normal road | `{ 150, 155, 165, 255 }` | width 4 px at zoom 1 |
| Shortcut road | `{ 230, 190, 60, 255 }` | draw dashed |
| Blocked road (`EdgeStatus::Blocked`) | `{ 220, 50, 50, 255 }` | plus a red "X" at the midpoint |
| Restricted road (`EdgeStatus::Restricted`, `speedCap > 0`) | `{ 240, 140, 40, 255 }` | speed-limit road |
| Oil spill (`effDifficulty > difficulty`) | `{ 120, 70, 160, 255 }` | purple overlay on top of the road |
| Highlighted route | `{ 70, 200, 255, 255 }` | width 7 px, under the vertices |
| Start | `{ 60, 200, 90, 255 }` | square |
| Finish | `{ 255, 255, 255, 255 }` | chequered square |
| Checkpoint | `{ 70, 140, 255, 255 }` | circle with its order number inside |
| Intersection | `{ 110, 115, 125, 255 }` | small circle |
| Fuel station | `{ 255, 200, 0, 255 }` | "F" icon; grey when `active == false` |
| Nitro pickup | `{ 0, 230, 230, 255 }` | "N" icon; grey when `active == false` |
| Dead end | `{ 160, 90, 60, 255 }` | small triangle |
| Human car | `{ 255, 80, 80, 255 }` | always drawn on top, with a white outline |
| Opponent cars | 9 distinct colours by racer id | |

`Theme::Apply()` calls `GuiLoadStyleDefault()` and then `GuiSetStyle(DEFAULT, TEXT_SIZE, 18)`, plus any colours you like for raygui.

### 3.2 MapView (`ui/MapView.h/.cpp`) — used by Explorer, Race and Lab

A small class that draws a `GraphView` inside a screen rectangle.

```cpp
class MapView
{
public:
    void SetBounds(Rectangle screenArea);             // where on screen the map goes
    void FitTo(const GraphView& g);                    // centre + zoom so all vertices fit (10% margin)
    void HandleInput();                                // wheel = zoom at cursor, right/middle drag = pan
    void Begin();                                      // BeginScissorMode + BeginMode2D(m_camera)
    void End();                                        // EndMode2D + EndScissorMode

    void DrawRoads(const GraphView& g, const Color* edgeOverride);  // edgeOverride may be nullptr
    void DrawVertices(const GraphView& g, const Color* vertexOverride, bool showIds);
    void DrawPath(const GraphView& g, const int* edgeIds, int count, Color c, float width);
    void DrawVertexPath(const GraphView& g, const int* vertexIds, int count, Color c, float width);

    int VertexAt(const GraphView& g, Vector2 mouse) const;   // -1 if none (within 12 px on screen)
    int EdgeAt(const GraphView& g, Vector2 mouse) const;     // -1 if none (within 8 px of the segment)
    Vector2 WorldToScreen(Vector2 p) const;                  // GetWorldToScreen2D
    Vector2 ScreenToWorld(Vector2 p) const;                  // GetScreenToWorld2D

private:
    Rectangle m_area;
    Camera2D m_camera;
};
```

Details:
- **Coordinates:** vertex `x, y` are world units from the `.trk` file. Draw them as they are (y grows downward, like the screen). `FitTo` computes the vertex bounding box and sets `m_camera.zoom = min(area.width / boxW, area.height / boxH) * 0.9`, `target` = box centre and `offset` = area centre.
- **Roads:** for each edge, draw a line from `vertices[from]` to `vertices[to]` (`DrawLineEx`). Choose the colour with this priority: `edgeOverride` (if non-null and its alpha is > 0) → Blocked → Restricted → oil spill → shortcut → normal. If `directed` is true, draw a small arrowhead at 60% along the line (`DrawTriangle`).
- **Cost labels:** if `Settings.showCostLabels` is on, draw `distance` (rounded, `%.0f`) at each road's midpoint in small text.
- **Vertices:** icon by `type` (section 3.1). Checkpoints show `checkpointOrder` inside the circle. Inactive services (`active == false`) are grey.
- **Hit-testing:** `VertexAt` converts each vertex to screen space and returns the nearest within 12 px. `EdgeAt` uses raylib's `CheckCollisionPointLine(mouse, a, b, 8)` on screen-space endpoints.
- **Path drawing helpers:** `ReachabilityView.path` and `RouteCandidateView.vertexPath` give **vertex** ids. To draw them as roads, write a drawing helper `int EdgeBetween(const GraphView& g, int a, int b)` that returns the first edge with `(from == a && to == b) || (!directed && from == b && to == a)`, or -1. That's a display lookup, not route planning.
- Vertex and edge ids equal their index in the arrays (`vertices[id].id == id`), so `g.vertices[id]` is O(1).

### 3.3 Format (`ui/Format.h`)

```cpp
void FormatTimeMs(int ms, char* out, int cap);        // 83456 -> "1:23.456"; INT_MAX -> "—"; -1 -> "—"
void FormatTenths(int tenths, char* out, int cap);    // 123 -> "12.3"
const char* PresetName(Preset p);                     // "Balanced", "Speed", "Economy", "Safety"
const char* StatusName(int resultStatus);             // 0 "Finished", 1 "DNF", 2 "Time limit"
const char* RacerStateName(RacerState s);
const char* TieBreakName(TieBreak t);                 // "Points", "Wins", "Sum of finish positions",
                                                      // "Best fastest lap", "Racer id", "Same racer"
const char* VertexTypeName(VertexType t);
```

### 3.4 Toast (`ui/Toast.h`)

`Toast::Show(const char* text)` copies the text into a static `char[256]` and shows it bottom-centre for 3 seconds, fading out. Use it everywhere a call returns an `ApiStatus` other than `ApiStatus::Ok`:

```cpp
ApiStatus s = ApiCmdNitro();
if (s != ApiStatus::Ok) Toast::Show(ApiLastError());
```

---

## 4. Navigation flow

```
                    ┌──────────────── Main Menu ────────────────┐
                    │ Championship │ Quick Race │ Algorithm Lab │ Instructions │ Settings │ Exit
                    └──────┬──────────────┬────────────┬──────────────┬────────────┬──────┘
                           │              │            │              │            │
   ApiChampionshipStart()  │              ▼            ▼              ▼            ▼
                           │       Circuit Select     Lab        Instructions   Settings
                           │       (C1, C2, C3, Generated)
                           ▼              │
                       Explorer ◄─────────┘   (inspect graph, routes, resources)
                           │ "Start race"
                           ▼
                         Race ──── "Open Lab" (pauses the race) ──► Lab ── Back ──► Race
                           │ ApiRaceOver()
                           ▼
                        Results
            Quick Race: "Race again" → Race · "Explorer" → Explorer · "Menu" → Main Menu
          Championship: "Continue" → ApiChampionshipNextRace() → Championship Dashboard
                           │
       Championship Dashboard ── not complete: "Next race" → Explorer (next circuit is already loaded)
                              └─ complete: champion banner, "Main Menu"
```

The brief allows "a sensible navigation flow", so this layout is fine. What matters is that all functions of all 5 screens can be reached.

---

## 5. Screen 1 — Main Menu (+ Instructions, Settings)

### 5.1 Main Menu (`MainMenuScreen`)

Layout: game title at the top, a vertical column of six 320×56 buttons in the centre, and the API version in small text at the bottom (`"Backend API v%d"`, `SMARTRACE_API_VERSION`).

| Button | Action |
|---|---|
| **Championship** | If `ApiChampionshipGetStatus().active && !complete`, label it "Continue Championship" and go to `Championship`. Otherwise call `ApiChampionshipStart()` (on failure, toast), set `mode = Championship`, `selectedCircuit = 0`, then `Go(Explorer)`. |
| **Quick Race** | `mode = QuickRace`, `Go(CircuitSelect)`. **Disable it** (`GuiDisable()`/`GuiEnable()`) while a championship is in progress and not complete: loading another circuit would break the championship's circuit order. Show a tooltip line: "Finish the championship first". |
| **Algorithm Lab** | `labOpenedFromRace = false`, `labReturnScreen = MainMenu`, `Go(Lab)`. |
| **Instructions** | `Go(Instructions)` |
| **Settings** | `Go(Settings)` |
| **Exit** | `RequestQuit()` |

If `ApiInit` failed in `App::Init`, show `ApiLastError()` in red under the title and disable everything except Exit.

### 5.2 Instructions (`InstructionsScreen`)

A scrollable text panel (`GuiScrollPanel`) with this content (write it as a `const char* const kLines[]` array in the .cpp). The numbers are the frozen backend rules, so copy them exactly:

- Goal: drive **3 laps** (2 or 3 in Settings), passing **checkpoints 1→10 in order** every lap, then cross the Finish. Fastest total time wins.
- Fuel: tank **100 units**. Each road burns its fuel cost × (1 + 0.1 × (difficulty − 1)). With no fuel for the next road, you stop.
- Fuel stations: buy up to **40 units** per stop. Any purchase costs a fixed **3.0 s** stop.
- Nitro: start with **1** charge, max **3**. Activation gives **1.5× speed for 4 s** and burns **2× fuel** meanwhile. Collect charges at nitro pickups.
- Events: road blocks and reopenings, oil spills (+2 difficulty for **20 s**), speed restrictions (50% cap for **15 s**), fuel-station outages (**30 s**), nitro boosters (**25 s** or until collected). Routes are recalculated automatically after every change.
- Recovery: a car with no route waits. If it is still stuck after **60 s**, it is out (DNF).
- Race end: the race stops **60 s** after the leader finishes (hard cap 15 min). Unfinished racers are classified but score 0.
- Points: **25-18-15-12-10-8-6-4-2-1**, plus **1** for the fastest lap.
- Route presets: **Balanced**, **Speed** (time first), **Economy** (fuel first), **Safety** (avoids difficulty and obstacles). The cost shown is exactly the cost the planner minimises.
- Controls: section 8.6.

### 5.3 Settings (`SettingsScreen`)

On `Enter`: `Settings s = ApiGetSettings();` and keep it in a member. Controls:

| Field | Control | Range |
|---|---|---|
| `laps` | `GuiToggleGroup` "2;3" | 2..3 |
| `opponentCount` | `GuiSpinner` | **3..9** (the API doesn't clamp, so the GUI must) |
| `defaultPreset` | `GuiToggleGroup` "Balanced;Speed;Economy;Safety" | enum order = `Preset` order |
| `eventSeed` | `GuiValueBox` (int) | ≥ 0 |
| `showCostLabels` | `GuiCheckBox` | |
| `startNitroCharges` | **Don't show it.** The engine always uses the frozen rule (1). Keep the value unchanged. | |

Buttons: **Save** → `ApiSetSettings(s)`, toast "Settings saved", `Go(MainMenu)`. **Cancel** → `Go(MainMenu)`.

Settings take effect at the **next** `ApiRaceStart` / `ApiChampionshipStart` (opponent count is fixed for a whole championship). Say so in a small note on the screen.

---

## 6. Circuit Select (`CircuitSelectScreen`)

Four cards in a row:
- For `i = 0..ApiCircuitCount()-1`: title `ApiCircuitName(i)`. On click: `ApiLoadCircuit(i)` (toast on failure), `selectedCircuit = i`, `Go(Explorer)`.
- A fourth card, **"Generated track"**, enabled only when `generatedTrackSaved` is true. On click: `ApiLoadCircuitFile(kGeneratedTrackPath)`, `selectedCircuit = -1`, `Go(Explorer)`.

Optional preview: after loading, draw a small `MapView` of `ApiGetGraph()` on each card. Simplest: load each circuit once in `Enter`, render it into a `RenderTexture2D`, and keep the three textures.

---

## 7. Screen 2 — Track Explorer / Route Planner (`ExplorerScreen`)

**Brief:** "Display graph, reachable checkpoints, alternative paths, route costs and route comparison" (BFS, DFS, Dijkstra, A*). The step "review resource recommendations" (§2.2) also lives here.

### 7.1 Layout

- Left ~65%: `MapView` of `ApiGetGraph()`. Call `FitTo` in `Enter`.
- Right ~35%: a tab bar (`GuiTabBar` or a `GuiToggleGroup`) with tabs **Overview · Reachability · Structure · Routes · Resources**, the active tab's panel under it, and at the bottom the buttons **Start race** (big), **Open in Lab** and **Back**.
- Top bar: circuit name, plus `Championship — race X of 3` when `mode == Championship` (from `ApiChampionshipGetStatus()`).

Keep two per-screen arrays, `Color m_vertexColour[256]` and `Color m_edgeColour[512]` (alpha 0 = "no override"). Each tab fills them, and `Draw` passes them to `MapView`. Clear both when switching tabs.

Find the Start vertex once in `Enter` (the vertex with `type == VertexType::Start`). It is the default BFS source.

### 7.2 Tab: Overview

`TrackInfo info = ApiGetTrackInfo();` shows: name, `vertexCount`, `edgeCount`, `mandatoryCheckpointCount`, `shortcutCount`, `laps`, has cycle, has dead end, and a big **VALID / INVALID** badge from `validatorPassed`.

Below it, the validator checklist: `ValidationView v = ApiValidateTrack();`. For each `v.checks[i]`, draw a ✓/✗ icon (green/red), `name`, and when it failed, `detail` in smaller red text. There are 11 checks.

Also show a small colour legend (section 3.1), because this is the first screen with a map.

### 7.3 Tab: Reachability (BFS)

- Source = Start by default. **Left-click** a vertex to set the source, **Shift + left-click** to set a target, and press **C** to clear the target.
- Whenever source or target changes: `ReachabilityView r = ApiRunBfs(source, target);` (target -1 when unset).
- Colour every vertex where `r.reachable[v]` is false dark grey, and the source green.
- Show "Reachable checkpoints:" followed by the list `r.reachableCheckpoints` (these are checkpoint **order numbers**, 1..10), for example "1 2 3 4 5 6 7 8 9 10".
- With a target: if `r.found`, draw `r.path` with `DrawVertexPath` and print "Fewest roads: N" (N = `r.path.count - 1`). Otherwise print "Target not reachable from source".
- Explain under it in one line: "BFS finds the path with the fewest roads, not the shortest distance."

### 7.4 Tab: Structure (DFS)

Three sections, each with a button that runs the query:
- **Components:** `ComponentsView c = ApiRunDfsComponents();` shows `componentCount` and colours vertices by `c.componentId[v]` (use a palette of 6 colours, cycling).
- **Cycle:** `CycleView cy = ApiRunDfsCycles();` shows "Has cycle: yes/no". If yes, draw `cy.witnessCycle` as a closed vertex path (also connect the last vertex back to the first).
- **Dead ends:** `DeadEndsView d = ApiRunDeadEnds();` lists `d.deadEndVertices` and marks them on the map with a brown ring.

### 7.5 Tab: Routes (Dijkstra / A*, route comparison)

Top part, **single lap plan**:
- Controls: preset `GuiToggleGroup` "Balanced;Speed;Economy;Safety", algorithm `GuiToggleGroup` "Dijkstra;A*".
- On any change: `RouteView rv = ApiPlanLap(preset, algo);`
- If `rv.found`: draw `rv.edgePath` with `DrawPath` in the highlight colour, and show **Cost (optimised): `totalCost` (3 decimals)**, distance, time (s), fuel (units). The cost label must say this is the value the algorithm minimised (brief §3.3).
- If not found: "No route: leg `blockedLegIndex` is blocked" (leg 0 = Start→CP1, ... last = CP10→Finish).
- Tip for the demo: switching Dijkstra ↔ A* gives the **same cost**. That is the point; A* just expands fewer vertices, which the Lab shows.

Bottom part, **route comparison** (Challenge 2):
- Control: "Compare under" preset `GuiToggleGroup`.
- `RouteCompareView cmp = ApiCompareRoutes(evalPreset);`
- A table with up to 4 rows: `label`, cost, distance, time, fuel. Mark the row with the lowest cost "best" (finding the minimum of ≤ 4 numbers for display is fine). Clicking a row draws its `vertexPath` on the map, each candidate in its own colour.
- One line of text: "All candidates are scored with the same preset so the comparison is fair. Different presets can pick different routes."

### 7.6 Tab: Resources (knapsack DP vs greedy, refuelling DP)

**Pickups:**
- Controls: preset toggle, and a budget `GuiSliderBar` 0..600 tenths (default 150, shown as "15.0 s").
- `PickupView p = ApiPlanPickups(preset, budget);`
- Table, one row per `p.items[i]`: vertex, type (`isNitro` ? "Nitro" : "Fuel"), cost (s), benefit (s), detour (s), **DP ✓**, **Greedy ✓**.
- Totals: "DP: benefit `dpBenefit` for cost `dpCost` (`dpStates` table cells)" and "Greedy: benefit `greedyBenefit` for cost `greedyCost`". If `greedyBenefit < dpBenefit`, add "Greedy is not optimal here".
- If `p.fuelNeeded`: show "Refuelling is required to finish this race" in orange.
- On the map, ring the DP-chosen vertices green and the greedy-chosen ones yellow.

**Refuel plan:**
- `RefuelView rf = ApiPlanRefuel(preset);`
- If `!rf.feasible`: "No refuelling plan finishes the race on this route" (red).
- Else: "`stopCount` stop(s), `totalStopTimeMs/1000` s stopped, finishing with `finalFuelTenths/10` units". List the stops where `purchaseTenths > 0`: "V`vertex`: buy `purchaseTenths/10` units", and mark those vertices on the map.

### 7.7 Buttons

- **Start race:** `ApiStatus s = ApiRaceStart();` On Ok: `Go(Race)`. On failure: toast.
- **Open in Lab:** `labOpenedFromRace = false`, `labReturnScreen = Explorer`, `Go(Lab)`.
- **Back:** Quick Race → `CircuitSelect`; Championship → `MainMenu` (the championship stays active, and the menu shows "Continue Championship").

---

## 8. Screen 3 — Racing (`RaceScreen`)

**Brief:** "Display cars, roads, checkpoints, fuel, nitro, race time and dynamic events", with routing, resource planning and the event heap behind it.

### 8.1 Enter

```cpp
void RaceScreen::Enter(App* app)
{
    m_paused = false;
    m_speed = 1;                 // 1x, 2x, 4x, 8x
    m_accumulatorMs = 0.0f;
    m_autopilot = true;          // ApiRaceStart always starts the human on autopilot
    m_pauseAtDecisions = true;
    m_lastLogIndex = -1;
    m_logCount = 0;
    m_refuelPromptVertex = -1;
    m_showRefuelPopup = false;
    m_map.SetBounds(...);
    m_map.FitTo(ApiGetGraph());

    RaceSnapshot snap = ApiRaceSnapshot();
    AppState& st = app->State();
    st.racerCount = snap.racers.count;
    for (int i = 0; i < snap.racers.count && i < 10; ++i)
    {
        snprintf(st.racerNames[snap.racers[i].id], 32, "%s", snap.racers[i].name);
    }
}
```

`ApiRaceStart()` was already called by the screen that opened the race (Explorer or Results "Race again").

### 8.2 Stepping the simulation (exactly like this)

Race time advances in whole 20 ms ticks. Use an accumulator, step **one tick at a time**, and stop early when the human has to decide something:

```cpp
void RaceScreen::Update(App* app, float dt)
{
    HandleInput(app);                       // section 8.6
    if (ApiRaceOver())
    {
        app->Go(ScreenId::Results);
        return;
    }
    if (m_paused || m_showRefuelPopup) return;

    m_accumulatorMs += dt * 1000.0f * (float)m_speed;
    int ticks = (int)(m_accumulatorMs / 20.0f);
    if (ticks > 40 * m_speed) ticks = 40 * m_speed;   // after a window drag/stall, don't fast-forward
    m_accumulatorMs -= (float)ticks * 20.0f;
    if (m_accumulatorMs < 0.0f) m_accumulatorMs = 0.0f;

    for (int i = 0; i < ticks; ++i)
    {
        ApiRaceStep(1);
        if (ApiRaceOver()) break;
        if (HumanNeedsDecision())
        {
            OnHumanDecisionPoint();   // may pause and/or open the refuel pop-up
            break;
        }
    }
    PullLog();
}
```

Every frame **after** stepping, call these once and use the results only in this frame:
- `RaceSnapshot snap = ApiRaceSnapshot();` (cars, time)
- `GraphView g = ApiGetGraph();` (roads now show the live effects: blocked, restricted, oil, inactive stations)
- `ResultsView live = ApiRaceResults();` (the live running order, already sorted by the backend)

Store them in members (`m_snap`, `m_graph`, `m_live`) for `Draw()`. They stay valid because nothing else steps the race before `Draw`.

`PullLog()`:

```cpp
LogView log = ApiRaceLogSince(m_lastLogIndex);
for (int i = 0; i < log.lines.count; ++i)
{
    // copy into your own ring buffer of the last 200 lines: time + text
    m_lastLogIndex = log.lines[i].index;
}
```

### 8.3 The human car: autopilot vs manual

Find the human with `snap.racers[i].isHuman`.

**Autopilot ON** (the default): the backend drives the human, routes, reroutes and refuels automatically. The player can still press **Nitro** and change the **preset**.

**Autopilot OFF** (manual): the human car **stops at every vertex** and waits until you send `ApiCmdChooseEdge(edgeId)`. Race time keeps running while it waits, unless "pause at decisions" is on.

```cpp
bool RaceScreen::HumanNeedsDecision()
{
    if (m_autopilot) return false;
    const RacerView* h = Human();
    return h != nullptr && (h->state == RacerState::AtVertex || h->state == RacerState::Waiting);
}
```

`OnHumanDecisionPoint()`:
1. If the human is at an **active fuel station** (`g.vertices[h->currentVertex].type == VertexType::FuelStation && active`), and `h->state == RacerState::AtVertex`, and `m_refuelPromptVertex != h->currentVertex`, open the refuel pop-up (section 8.5) and set `m_refuelPromptVertex = h->currentVertex`.
2. Else, if `m_pauseAtDecisions`: set `m_paused = true` and show the banner "Choose your next road" (it unpauses when the choice is sent).
3. Reset `m_refuelPromptVertex = -1` whenever the human's state is `OnEdge` (so the next station visit asks again).

**Legal next roads** (for highlighting and click targets): an edge `e` is a candidate when `e.from == v || (!e.directed && e.to == v)`, **and** `e.status != EdgeStatus::Blocked`. This is only for display. The backend checks the move again, so always check the return value:

```cpp
ApiStatus s = ApiCmdChooseEdge(edgeId);
if (s == ApiStatus::Ok)
{
    m_paused = false;
}
else
{
    Toast::Show(ApiLastError());   // e.g. "illegal edge from here"
}
```

To choose, the player either **clicks a highlighted road** (`m_map.EdgeAt`) or presses **1–9** for the 1st..9th legal road (number the roads on the map next to each candidate).

Show the human's next checkpoint (`h->nextCheckpoint`) by pulsing that checkpoint's circle. The player needs it to choose roads.

If the human is in `RacerState::Waiting` (no fuel for the next road, or no route), show the reason from the log line and a "Waiting... (DNF after 60 s)" banner.

**Toggling autopilot:** `ApiCmdAutopilot(on)`, then set `m_autopilot = on`. Switching it ON again re-plans automatically.

### 8.4 HUD (always visible)

Top bar, left to right:
- Race time: `FormatTimeMs(snap.timeMs)`.
- Lap: `Lap h->lap / laps` (laps from `ApiGetTrackInfo().laps`).
- Next checkpoint: `CP h->nextCheckpoint`.
- Position: `P h->livePosition / snap.racers.count`.
- **Fuel bar:** width ∝ `h->fuelTenths / 1000`, the text "`fuelTenths/10` / 100". Red below 25%.
- **Nitro:** 3 slots, filled for `h->nitroCharges`. A glowing "NITRO" label while `h->nitroActive`.
- Preset: `GuiToggleGroup` "Balanced;Speed;Economy;Safety". On change: `ApiCmdPreset(p)`.
- Autopilot: `GuiToggle` "Autopilot".
- Speed: `GuiToggleGroup` "‖;1x;2x;4x;8x" (‖ = pause).
- Button **Open Lab** (section 8.8).

Right sidebar:
- **Live order:** for each row of `m_live.rows` (already in race order): position, `racerNames[racerId]`, laps completed, status. Highlight the human's row.
- **Event ticker:** the last ~12 log lines, newest at the bottom: `[m:ss] text`. Colour lines that contain "blocked" red, "refuel" yellow and "nitro" cyan (a substring check for colour is fine).

Map (centre):
- `DrawRoads(m_graph, ...)`. Live effects appear automatically through `status`, `effDifficulty` and `speedCap`, and through `active` on vertices.
- In manual mode while deciding: legal roads highlighted, plus their 1–9 numbers.
- Cars: for each racer, a 14×8 rectangle at `(x, y)` rotated by `heading * RAD2DEG` (`DrawRectanglePro`). The human is red with a white outline and drawn last. When several cars share a spot, offset each by `(id % 3 - 1) * 4` px perpendicular to the heading so they don't hide each other. That's purely cosmetic.
- A car whose `state` is `Finished` is drawn at the Finish with a small flag. `Dnf` cars are drawn grey with an "X".

### 8.5 Refuel pop-up (manual mode only)

A modal panel (`GuiPanel`) in the centre:
- Title: "Fuel station V`vertex`".
- "Tank: `fuelTenths/10` / 100".
- A slider for the purchase in tenths: min 0, max `min(400, 1000 - fuelTenths)`, **pre-filled with `ApiRecommendedRefuel()`** (the refuelling DP's recommendation), shown in units with one decimal.
- The line "Recommended by the refuelling DP: X units". "Any purchase costs a 3.0 s stop."
- Buttons: **Buy** → `ApiCmdRefuel(amount)`. On Ok, close; otherwise toast and keep it open. **Skip** → close without buying.
- After closing, if `m_pauseAtDecisions`, keep paused for the road choice (8.3 step 2).

On autopilot the backend refuels by itself, so never show the pop-up then.

### 8.6 Controls

| Key / mouse | Action |
|---|---|
| Space | pause / resume |
| N | `ApiCmdNitro()` |
| A | toggle autopilot |
| 1–9 | choose the n-th legal road (manual, while deciding) |
| Left-click a highlighted road | choose it |
| Mouse wheel / right-drag | zoom / pan |
| F | re-fit map |
| Tab | cycle preset |
| `+` / `-` | sim speed up / down |
| Esc | pause and show the "Quit race?" dialog → Main Menu (the race is abandoned) |

Put a one-line controls hint at the bottom of the screen.

### 8.7 Race over

When `ApiRaceOver()` becomes true, `Go(Results)`. Don't call `ApiRaceStep` again.

### 8.8 Lab from the race

**Open Lab** pauses (`m_paused = true`), sets `labOpenedFromRace = true` and `labReturnScreen = Race`, then calls `Go(Lab)`. The Lab now runs the algorithms on the **live** graph, with the current blocks and oil spills. This is the brief's "step through algorithms on the current track/data", and it makes a good viva demo: wait for the road block, open the Lab, run Dijkstra and show the detour. Coming back, the race resumes from where it was (`Enter` must **not** reset the race state when returning from the Lab: keep a `m_resumeFromLab` flag and skip the reset block in `Enter`).

---

## 9. Race Results (`ResultsScreen`)

In `Enter`, **copy** `ApiRaceResults()` into your own array right away. In a championship, the next step (`ApiChampionshipNextRace`) loads the next circuit and the race's results view goes away.

Table columns: Pos (`finishPosition`), Name (`racerNames[racerId]`), Status (`StatusName(status)`), Time (`finishTimeMs`, "—" if -1), Best lap (`bestLapTimeMs`, "—" if INT_MAX), Laps (`lapsCompleted`), Points (`pointsAwarded`), and "+1 fastest lap" when `bonusPoints > 0`. Highlight the human's row.

Buttons by mode:
- **Quick Race:** **Race again** → `ApiRaceStart()` then `Go(Race)`. **Explorer** → `Go(Explorer)`. **Main Menu**.
- **Championship:** a single **Continue** → `ApiStatus s = ApiChampionshipNextRace();`. On Ok, `Go(Championship)`. On failure, toast.

---

## 10. Screen 5 — Championship Dashboard (`ChampionshipScreen`)

**Brief:** "Race results, standings, statistics and top-k view" (Merge, Quick, Heap Sort, priority queue).

`ChampionshipStatus cs = ApiChampionshipGetStatus();`

### 10.1 Header

"Championship — after race `racesCompleted` of `racesTotal`". If `cs.complete`, show a large banner **"CHAMPION: name"**, where the name is the row whose `racerId == cs.championId` (from the standings).

### 10.2 Standings panel (left)

- Sort method: `GuiToggleGroup` "Merge Sort;Quick Sort;Heap Sort".
- `StandingsView sv = ApiStandings(method);` Call it when the method changes and in `Enter`, then copy the rows.
- Columns: Pos, Name, Points, Wins, Sum of positions, Best lap.
- Under the table: "**`comparisons`** comparisons · **`swaps`** moves/swaps · **`micros`** µs", and a badge: **"Merge = Quick = Heap ✓"** if `sv.allMethodsAgree`, red "✗" otherwise.
- Line: "All three sorts use the same comparator: points ↓, wins ↓, sum of finish positions ↑, best lap ↑, racer id ↑."

### 10.3 Top-k panel (right, Challenge 3)

- `k` slider 1..(number of racers), default 3.
- `TopKView tk = ApiTopK(k);`
- List `tk.rows` (best first).
- "Size-k heap: **`comparisons`** comparisons vs full Merge Sort: **`fullSortComparisons`**", and "Matches the sorted prefix ✓/✗" from `matchesSortPrefix`.
- A short sentence: "For small k the heap does less work than a full sort. As k approaches n, the advantage disappears."

### 10.4 Tie-break explainer

Two dropdowns (racer A, racer B) filled with the standings names. Show `TieBreakName(ApiTieBreakReason(a, b))` as "A is ahead of B on: Wins". It's the comparator criterion that decided their order.

### 10.5 Race history

Tabs "Race 1 / Race 2 / Race 3". Only tabs `< racesCompleted` are enabled. `ResultsView hv = ApiRaceHistory(i);` draws the same table as the Results screen.

### 10.6 Buttons

- If `!cs.complete`: **Next race: `ApiCircuitName(cs.currentCircuitIndex)`** → `Go(Explorer)`. The circuit is already loaded by `ApiChampionshipNextRace`.
- **Open Lab (sorts)** → `labReturnScreen = Championship`, `Go(Lab)` with the Merge Sort module preselected. While a championship is running, the Lab's sorts rank **these** standings.
- **Main Menu**.

---

## 11. Screen 4 — Algorithm Lab (`LabScreen`)

**Brief:** "Step through algorithms on the current track/data and show counters", using the same code as gameplay. The backend runs the **real** function once, with a recorder attached. You then ask for the state after any step. **You never re-run or simulate anything yourself.**

### 11.1 Layout

```
┌───────────────────────────────────────────────────────────────────────────────┐
│ [Module list]   │           Visualisation (map / array / table / events)      │
│ BFS             │                                                             │
│ DFS             │                                                             │
│ Dijkstra        │                                                             │
│ A*              │                                                             │
│ Kruskal (MST)   │                                                             │
│ Track Generator ├──────────────────────────────┬──────────────────────────────┤
│ Knapsack DP     │ Pseudocode (current line lit) │ Counters + step description  │
│ Greedy pickup   │                              │                              │
│ Refuel DP       │                              │                              │
│ Merge Sort      ├──────────────────────────────┴──────────────────────────────┤
│ Quick Sort      │ |◄  ◄  ▶/‖  ►  ►|   [====scrubber=====]  step 37 / 412  speed│
│ Heap Sort       │ Result: Dijkstra: cost 3.758 over 3 roads; 19 vertices ...   │
│ Event Queue     │                                                             │
│ [Inputs panel]  │                                                             │
│ [Run]  [Back]   │                                                             │
└───────────────────────────────────────────────────────────────────────────────┘
```

Module list order = `LabAlgo` enum order (Bfs, Dfs, Dijkstra, AStar, Kruskal, TrackGenerator, KnapsackDp, GreedyPickup, RefuelDp, MergeSort, QuickSort, HeapSort, EventQueue). Use `GuiListView` with the text `"BFS;DFS;Dijkstra;A*;Kruskal (MST);Track Generator;Knapsack DP;Greedy pickup;Refuel DP;Merge Sort;Quick Sort;Heap Sort;Event Queue"`.

A **circuit selector** ("C1;C2;C3") above the list: on change, `ApiLoadCircuit(i)`. **Hide it when `labOpenedFromRace`** (loading a circuit would end the race) **and while a championship is in progress and not complete** (it would load a circuit out of the championship's order). In `Enter`, if `ApiGetTrackInfo().vertexCount == 0` (nothing loaded), call `ApiLoadCircuit(0)`.

### 11.2 Inputs panel (per module)

Start from `LabInput m_input = ApiLabDefaultInput();` in the constructor and edit only the fields each module reads:

| Module | Inputs to show |
|---|---|
| BFS | source, target (click on map: left = source, Shift + left = target; "Default" button resets both to -1) |
| DFS | source |
| Dijkstra, A* | source, target, preset |
| Kruskal | — |
| Track Generator | seed (`GuiValueBox`, default 1), vertex count (`GuiSpinner` 20..40, default 28) |
| Knapsack DP, Greedy | preset, budget (slider 0..600 tenths), "Use the brief's A/B/C example" checkbox (`useBriefExample`) |
| Refuel DP | preset |
| Merge / Quick / Heap Sort | array size (2..64) and seed. These are only used when no championship is running; otherwise show "Sorting the current championship standings" and hide them. |
| Event Queue | — (uses the circuit's scripted events) |

Defaults for source/target are -1, which means Start, and the mid-lap checkpoint (CP5) for the target. Show the actual vertex ids next to the inputs.

**Run** button:

```cpp
ApiStatus s = ApiLabRun(m_algo, m_input);
if (s != ApiStatus::Ok)
{
    Toast::Show(ApiLastError());
    m_hasRun = false;
}
else
{
    m_hasRun = true;
    m_stepCount = ApiLabStepCount();
    m_step = -1;                    // start before the first step
    m_playing = false;
    m_code = ApiLabPseudocode(m_algo);   // copy the line pointers into your own array (they are static strings)
    snprintf(m_resultText, sizeof(m_resultText), "%s", ApiLabResultText());
    if (FrameKindOf(m_algo) == LabFrameKind::Graph) m_map.FitTo(ApiLabGraph());
}
```

Also re-run automatically when the module changes (after resetting the inputs that don't apply).

### 11.3 Playback controls

- Buttons: **|◄** (`m_step = -1`), **◄** (`m_step - 1`, min -1), **▶/‖** (play/pause), **►** (`m_step + 1`, max `m_stepCount - 1`), **►|** (`m_step = m_stepCount - 1`).
- Scrubber: `GuiSliderBar` from -1 to `m_stepCount - 1`. Dragging sets `m_step`.
- Speed: `GuiSliderBar` 1..200 steps per second (default 10). While playing, `m_stepAcc += dt * speed` and advance whole steps; stop at the end.
- Keys: Left/Right = step, Space = play/pause, Home/End.
- Text: "Step `m_step + 1` / `m_stepCount`".

Each frame, **exactly once**: `LabFrame f = ApiLabFrame(m_step);` and draw from `f` in this frame only. Its arrays are valid until the next `ApiLabFrame`/`ApiLabRun`. Seeking forward is cheap. Seeking backward replays from the start, which is still fast for these sizes.

### 11.4 Pseudocode and step text (all modules)

- Pseudocode panel: draw `m_code.lines[i]` monospace (raylib's default font is fine, or load a monospace TTF from `data/` if you add one). Highlight line `f.line` with a coloured bar. If `f.line == -1`, highlight nothing.
- Step description: build one line from `f.op`, `f.a`, `f.b`, `f.value`:

| `LabOp` | Meaning of a / b / value | Example text |
|---|---|---|
| VisitVertex | a = vertex, value = its distance/cost | "Visit V7 (dist 3)" |
| DiscoverVertex | a = vertex, b = parent, value = distance / discovery time | "Discover V9 from V4 (d = 2)" |
| FinishVertex | a = vertex, value = finish time | "Finish V9 (t = 14)" |
| ExamineEdge | a = edge | "Examine road E12" |
| RelaxEdge | a = edge, b = vertex, value = new cost | "Relax E12: V5 cost → 3.41" |
| PushHeap | Graph: a = vertex, value = key. Events: a = event id, b = type, value = time | "Push V5 (key 3.41)" / "Schedule event #3 at 40.0 s" |
| PopHeap | Graph: a = vertex, value = key. Events: a = event id | "Pop V5" / "Apply event #3" |
| SetParent | a = vertex, b = parent | |
| DpCell | a = row, b = column, value = cell value | "dp[3][10] = 48" |
| Compare | a, b = array indices (-1 = the pivot) | "Compare A[2] with A[5]" |
| Swap | a, b = indices | "Swap A[2] ↔ A[5]" |
| Place | Array: a = index, b = key. Table: a = item row, b = item id. Generator: a = vertex, b = role, value = checkpoint order | "Write 42 to A[3]" / "Take item 1" / "Place CP4 at V17" |
| Backtrack | a = vertex | "Backtrack: undo V17" |
| Reject | Kruskal: a = edge. Greedy: a = item id. Events: a = event id (cancelled) | "Reject E8 (would close a cycle)" |
| UnionSets | a, b = set representatives | "Union {3} ∪ {8}" |
| AcceptEdge | a = edge | "Accept E8 into the tree" |
| MarkEdge | a = edge, b = new state code | "E8 → shortcut" |

At `f.step == -1`, show "Initial state".

### 11.5 Counters panel

Show the counters that matter for the module (all are running totals up to the current step):

| Module | Counters to show |
|---|---|
| BFS, DFS | `visited`, `examined` |
| Dijkstra, A* | `visited` (expanded), `relaxed`, `heapPushes`, `heapPops`, `examined` |
| Kruskal | `examined`, `rejections`, accepted = count of `edgeState == 2` (counting for display is fine) |
| Track Generator | `placements`, `backtracks`, `rejections` |
| Knapsack DP, Refuel DP | `tableCellsFilled` |
| Greedy | `placements` (taken), `rejections` (skipped) |
| Sorts | `comparisons`, `swaps` (moves for Merge Sort), `placements` |
| Event Queue | `heapPushes` (scheduled), `heapPops` (applied), `rejections` (cancelled) |

Under them, the result line from `ApiLabResultText()`.

### 11.6 Visualisation by `f.kind`

**`LabFrameKind::Graph`** (BFS, DFS, Dijkstra, A*, Kruskal, Generator)

- Draw `GraphView lg = ApiLabGraph();` with `MapView`. **Always use `ApiLabGraph()` here, not `ApiGetGraph()`**: for the generator it is the candidate-road graph of the accepted attempt. `f.vertexState.count == lg.vertices.count` and `f.edgeState.count == lg.edges.count`.
- Vertex colour from `f.vertexState[v]` (search modules): 0 unseen = grey, 1 discovered / in frontier = yellow, 2 visited / settled = green, 3 finished (DFS) = dark blue. Draw `f.vertexValue[v]` next to each vertex where it is ≥ 0 (BFS hops, DFS discovery time, Dijkstra/A* cost with 2 decimals).
- Parent tree: for each v with `f.vertexParent[v] >= 0`, draw the edge `EdgeBetween(lg, parent, v)` thicker in light green.
- Edge colour from `f.edgeState[e]`: 0 untouched = faint grey, 1 examined = white, 2 relaxed / tree / accepted = green, 3 rejected = red dashed.
- Pulse the vertex or edge the current step touches (`f.a`, per the table in 11.4).
- **Generator** uses different codes. Vertex roles: 0 none, 1 Start, 2 Finish, 3 Checkpoint (`vertexValue` = its order), 4 Fuel station, 5 Nitro. Draw them with the normal vertex icons from section 3.1. Edge codes 4 extra road (blue), 5 shortcut (gold dashed), 6 oil spill (purple), 7 road block (red); codes 0–3 are as above (Kruskal phase).

Legend box in the corner for the current module.

**`LabFrameKind::Array`** (Merge, Quick, Heap Sort)

- `f.arrayValues` = the keys in their current order (racer ids when sorting the standings, random 0..99 otherwise). Draw a bar chart: one bar per element, height ∝ value, value printed on the bar.
- Highlight `f.a` and `f.b` when the op is Compare (yellow), Swap (red) or Place (green). Index -1 is the quick-sort pivot: show "pivot" in the step text, with no bar.
- At the last step the array is sorted. Show "Sorted ✓" there (the backend's tests guarantee it).

**`LabFrameKind::Table`** (Knapsack DP, Greedy, Refuel DP)

- `f.tableRows × f.tableCols`, row-major in `f.tableCells` (index `r * tableCols + c`). -1 = not filled yet.
- Knapsack: rows = items 0..m, columns = budget 0..B. Refuel: rows = stops, columns = fuel level in tenths. Tables can be wide (refuel has 1001 columns): draw them in a `GuiScrollPanel`, only the visible cells (compute the first and last visible row/column from the scroll offset), with cells about 34×22 px. **Auto-scroll** so the current cell (`f.a`, `f.b` when `f.op == LabOp::DpCell`) stays visible.
- Cell colours: unfilled = dark, filled = grey with its value, current = yellow.
- Items taken: `f.selectedIds` (item ids). List them under the table as "Taken: A, B" (for the brief's example, ids 0/1/2 = A/B/C). Greedy has `tableRows == 0`: show only the taken (`selectedIds`) and skipped (`rejectedIds`) item lists.
- With "Use the brief's example": the DP ends at `dp[3][10] = 48` (B + C), while greedy takes only A (30). It's a good viva point.

**`LabFrameKind::Events`** (Event Queue)

- `ArrayView<LabEventView> ev = ApiLabEvents();` gives every scheduled event in schedule order: id, type, time, priority, sequence, target.
- Table rows = events. Status column per row:
  - "not scheduled yet" when its row index ≥ `f.heapPushes` (events are scheduled in list order),
  - "cancelled" when its id is in `f.rejectedIds`,
  - "applied #n" when its id is at position n in `f.selectedIds` (the order the heap popped them),
  - otherwise "in heap".
- A caption: "Ordered by (time, priority, insertion sequence). Cancelled events stay in the heap and are skipped when popped (lazy deletion)."

### 11.7 Generator extras

After a TrackGenerator run:
- `GeneratorReportView rep = ApiLabGeneratorReport();` lists every attempt: "Attempt n: ACCEPTED/rejected — `reason` (`nodesExplored` tried, `prunedBranches` pruned)". Show rejected ones in red. **For the viva (Challenge 1): seed 2** has attempt 0 rejected ("Feasible fuel plan: no refuelling plan finishes 3 laps...") and attempt 1 accepted. **Seed 3** has 9 rejections before acceptance.
- The steps you play back belong to the **accepted** attempt.
- Button **"Save & play this track"** (enabled when `rep.acceptedAttempt >= 0`):
  ```cpp
  if (ApiLabSaveGenerated(kGeneratedTrackPath) == ApiStatus::Ok && ApiLoadCircuitFile(kGeneratedTrackPath) == ApiStatus::Ok)
  {
      app->State().generatedTrackSaved = true;
      app->State().selectedCircuit = -1;
      app->State().mode = GameMode::QuickRace;
      app->Go(ScreenId::Explorer);          // the Overview tab shows its validator checklist
  }
  else
  {
      Toast::Show(ApiLastError());
  }
  ```
  Disable this button when `labOpenedFromRace` (it would end the race), and during a running championship.

### 11.8 Back

`Go(labReturnScreen)`.

---

## 12. Milestones (17 Nov → 5 Dec)

| # | Dates | Build | Done when |
|---|---|---|---|
| G0 | Tue 17 – Wed 18 Nov | §1 setup, §2 skeleton, Theme, Format, Toast, Main Menu, Instructions, Settings | Window opens, menu navigates to every screen (empty screens are fine), settings save via `ApiSetSettings`. |
| G1 | Thu 19 – Sat 21 Nov | MapView, Circuit Select, **Explorer** (all 5 tabs) | All 3 circuits draw correctly; BFS reachability, cycle, dead ends, lap route + comparison, pickups and refuel plan all show real numbers. Presets pick different routes on at least one circuit. |
| G2 | Sun 22 – Wed 25 Nov | **Race** (autopilot first, then manual, refuel pop-up, nitro, HUD, ticker), Results, Quick Race flow | A full 3-lap autopilot race runs to the Results screen on each circuit; a manual race can be driven by clicking roads; road-block and oil-spill events visibly change roads. |
| G3 | Thu 26 – Fri 27 Nov | **Championship** flow + Dashboard | 3 races in a row → champion banner; all 3 sort methods agree; top-k panel and race history work. |
| G4 | Sat 28 Nov – Tue 1 Dec | **Algorithm Lab**: Graph kind first, then Array, Table, Events, Generator + Save & play | Every module runs, steps forwards and backwards, highlights the pseudocode line and shows counters. Lab from race shows the live graph. |
| — | ≈ Mon 30 Nov | **Joint walkthrough** (≈1 h) | Wasay explains the 12 algorithms and the race engine; Rayyan explains every screen and which API call feeds it. Both of you must be able to answer viva questions on both halves. |
| G5 | Wed 2 – Thu 3 Dec | Polish, §14 test checklist, **Release** build, README GUI section, report sections (§13) | Checklist all ticked on a Release build. |
| G6 | Fri 4 Dec | Joint packaging: clean-folder build from the ZIP | ZIP extracts, configures, builds and runs on a clean folder. One day of buffer. |

If you fall behind, cut **polish**, never **functions**. Every item listed in the brief for a screen must be reachable, even if it looks plain.

---

## 13. Your deliverables (besides the code)

1. **README — "GUI" section** (add under the existing backend text): how to run `smartrace.exe` (from its folder, with `data/` next to it), the controls table (§8.6), and the screen flow (§4).
2. **Acknowledgements** (README and report): "raylib 5.5 (zlib/libpng licence, raysan5) and raygui 4.0 (zlib/libpng licence, raysan5) are used for windowing, drawing and UI widgets. No other third-party code."
3. **Report sections you write:** title page with both names and roll numbers; introduction; "System architecture and game flow" (the §4 diagram, the API boundary, a screenshot of each screen with one paragraph each); limitations (GUI side); conclusion. Add a screenshot of each of the 5 screens and of the Lab in each frame kind.
4. **Release build:** `cmake --build build --config Release`; the exe + `data/` from `build/Release/` go in the ZIP's `bin/` folder.
5. **ZIP:** `24I-3071_24I-3000_<Section>.zip` containing the source, `third_party/`, `data/`, `bin/` (Release exe + data), `results/`, the report PDF and the README. No `build/` folder. Test it on a clean folder on Fri 4 Dec.

---

## 14. Manual test checklist (tick on the Release build)

**Menu & settings**
- [ ] Every menu button works. Quick Race is disabled while a championship is running.
- [ ] Opponents 3 and 9 both start races with that many cars. Laps 2 and 3 both work.

**Explorer** (on each of C1, C2, C3)
- [ ] The validator shows 11 green checks.
- [ ] BFS from Start reaches all 10 checkpoints. Picking an unreachable target says so.
- [ ] Cycle witness drawn, dead ends marked, 1 component.
- [ ] Dijkstra and A* give the same cost for every preset.
- [ ] At least one circuit shows different routes for different presets.
- [ ] Pickup table: DP benefit ≥ greedy benefit. With budget 0 nothing is chosen.

**Race**
- [ ] Autopilot race finishes → Results; positions and points are plausible.
- [ ] Pause, 1x/2x/4x/8x all work. Results are identical at any speed (same event seed = same result).
- [ ] Manual mode: the car stops at each vertex, only legal roads are clickable, an illegal click shows a toast.
- [ ] Refuel pop-up appears at a station in manual mode, pre-filled with the recommendation; Buy adds fuel (3 s stop); Skip continues.
- [ ] Nitro: a charge is used, the speed boost is visible, and it is refused with 0 charges (toast).
- [ ] The road-block event turns a road red and cars reroute without the race restarting. The oil spill shows purple. The ticker shows the events.
- [ ] Open Lab mid-race → Dijkstra runs on the live graph → Back resumes the race.

**Championship**
- [ ] 3 races in order C1 → C2 → C3, then the champion banner.
- [ ] Merge/Quick/Heap give the same table; the agreement badge is green.
- [ ] Top-k matches the top of the table for k = 1 and k = n.
- [ ] Race history tabs show each race; tie-break explainer gives a reason.

**Lab**
- [ ] All 13 modules run on C1; stepping to -1 and to the end works; scrubbing back and forth gives consistent pictures.
- [ ] The pseudocode highlight moves with the steps; counters only go up as you step forward.
- [ ] Brief example: DP 48 vs greedy 30.
- [ ] Generator seed 2: attempt 0 rejected with a reason, attempt 1 accepted; Save & play → Explorer shows a valid track → it races.
- [ ] Sorts rank the standings during a championship, random keys otherwise.

**Robustness**
- [ ] Resizing the window doesn't break layouts (recompute rectangles from `GetScreenWidth()/GetScreenHeight()` each frame).
- [ ] No crash when clicking buttons in odd orders (for example, Lab Run with nothing selected).
- [ ] Code check before each commit: no `std::`, no STL headers, Allman braces (search `src/app` for `std::` and `#include <` and fix anything that isn't a C header, raylib or raygui).

---

## 15. Viva demo script (about 8 minutes)

1. **Menu → Quick Race → C1 Harbor → Explorer.** Overview: 11 validator checks. Reachability: all 10 checkpoints reachable (BFS). Structure: cycle and dead end (DFS).
2. **Routes:** switch presets and point out the different routes; Dijkstra vs A* give the same cost. Compare table (Challenge 2).
3. **Resources:** DP vs greedy and the refuel plan.
4. **Start race (autopilot).** At ~40 s the road block appears; the cars reroute (event heap + Dijkstra). Use nitro. Turn autopilot off and drive a few vertices by hand; refuel via the pop-up (refuel DP recommendation).
5. **Open Lab from the race:** Dijkstra on the live graph, then A* (fewer expanded vertices, same cost). Back to the race; let it finish → Results.
6. **Lab:** Knapsack brief example (48 vs 30). Event Queue (cancelled event skipped). Kruskal (rejected cycle edges).
7. **Generator seed 2:** the rejected attempt and its reason (Challenge 1), step through the placement backtracking, Save & play.
8. **Championship:** standings by all three sorts, top-k (Challenge 3), tie-break reason.

---

## 16. FAQ / gotchas

- **"The view I stored has garbage in it."** You kept a borrowed view past its lifetime. Copy the values, or call the API again this frame.
- **"Racer names in results are missing."** `ResultRowView` has only `racerId`. Use the `racerNames` you copied from the snapshot in `RaceScreen::Enter`. Standings rows have `name`.
- **"ApiCmdChooseEdge says 'autopilot is on'."** Call `ApiCmdAutopilot(false)` first. A new race always starts with autopilot on.
- **"The refuel pop-up keeps reopening."** Use `m_refuelPromptVertex` (§8.3). A station can be used once per visit.
- **"The Lab shows a different graph from the Explorer."** For the generator, that's intended (`ApiLabGraph()` = candidate roads). For other modules, it's the loaded circuit.
- **"I need a number the API doesn't give."** Ask Wasay. Don't compute it in the GUI. Additions to the API are allowed if they only add things; nothing existing will change.
- **Release crashes but Debug works:** usually an uninitialised member. Initialise every member in each constructor.
- **raygui returns:** `GuiButton` returns non-zero on the click frame. `GuiToggleGroup`, `GuiSliderBar`, `GuiSpinner` and `GuiCheckBox` write through the pointer you pass. `GuiSpinner`/`GuiValueBox`/`GuiTextBox` need an `editMode` bool that you toggle when they return non-zero.
