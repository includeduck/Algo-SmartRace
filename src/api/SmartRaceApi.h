#pragma once

#include "ApiTypes.h"

// ===========================================================================
// SmartRace backend API — v0.1 (Phase 0 / P0)
//
// This is the ONLY header the GUI includes from the backend, together with
// ApiTypes.h which it pulls in. The GUI must not reach into src/core,
// src/model, src/algo, src/sim or src/io directly — every number a screen
// needs is computed here and handed back as a plain struct.
//
// THREADING: single-threaded. Call these only from the GUI's main loop. The
//            backend never calls back into the GUI.
// LIFETIME:  every returned view/pointer is borrowed. It stays valid until
//            the next call noted in that function's own comment (usually:
//            the next call that can change the same data). Copy out
//            anything you need to keep past that.
// ERRORS:    any call that can fail returns ApiStatus; ApiLastError() gives
//            a short, human-readable reason to show the player. No call
//            crashes on bad input (unknown id, nothing loaded yet, ...) —
//            it returns a status instead.
// TIME:      the GUI advances race time only by calling ApiRaceStep(ticks).
//            Results never depend on frame rate or wall-clock time.
//
// CHANGELOG
//   v0.1 (Phase 0): Init/shutdown, circuit list, static graph view,
//     settings, and a STUB race (one hard-coded loop circuit, a few fake
//     racers moving deterministically) so the GUI can be built and driven
//     end to end immediately.
//   v0.2 (Phase 1): ApiLoadCircuit/ApiGetGraph/ApiGetTrackInfo now load the
//     real 3 circuits via TrackLoader and reflect the real Graph. Added the
//     Track Explorer queries (BFS reachability, DFS components/cycles/dead
//     ends) and route planning (Dijkstra/A*, checkpoint-aware PlanLap,
//     CompareRoutes). Racing (ApiRaceStart/Step/Snapshot) is STILL the
//     Phase 0 stub — the real race engine replaces it in Phase 2 without
//     changing its signatures. Resource planning, championship and the
//     Algorithm Lab are added in later phases as pure ADDITIONS to this
//     header. API freeze: 29 Nov 2026.
// ===========================================================================

#define SMARTRACE_API_VERSION 2

// ---------------------------------------------------------------------- //
// Session / Main Menu
// ---------------------------------------------------------------------- //

// Loads rules.cfg and the circuit list from dataDir. Call once before any
// other Api* function. Calling it again resets everything (as if the app
// had just started).
ApiStatus ApiInit(const char* dataDir);
void ApiShutdown();

// A short reason for the last ApiStatus != Ok returned by any Api* call.
// Valid until the next Api* call.
const char* ApiLastError();

int ApiCircuitCount();
// Valid until ApiShutdown.
const char* ApiCircuitName(int index);

// Loads circuit `index` as the current track, replacing whatever was
// loaded. Ends any race in progress.
ApiStatus ApiLoadCircuit(int index);

// Loads a .trk file directly (used for track-generator output in Phase 4).
// Not implemented in v0.1.
ApiStatus ApiLoadCircuitFile(const char* path);

Settings ApiGetSettings();
ApiStatus ApiSetSettings(const Settings& settings);

// ---------------------------------------------------------------------- //
// Static track data — every screen that draws the map uses this
// ---------------------------------------------------------------------- //

// Valid until the next ApiLoadCircuit*/ApiRaceStep call (an edge's
// *effective* fields can change once a race with active events is running).
GraphView ApiGetGraph();
TrackInfo ApiGetTrackInfo();

// ---------------------------------------------------------------------- //
// Racing — STUB in v0.1 (one hard-coded circuit, fake deterministic
// racers). The real deterministic race engine replaces this body in
// Phase 2 without changing these signatures.
// ---------------------------------------------------------------------- //

ApiStatus ApiRaceStart();

// Advances the simulation by `ticks` steps of Rules::kSimStepMs each.
void ApiRaceStep(int ticks);

// Valid until the next ApiRaceStep call.
RaceSnapshot ApiRaceSnapshot();

bool ApiRaceOver();

// ---------------------------------------------------------------------- //
// Track Explorer / Route Planner (Screen 2) — Phase 1
// ---------------------------------------------------------------------- //

// BFS reachability/min-edge path from `source`. Pass target = -1 for full
// reachability only (ApiRunBfs(src, -1).found is always false; read
// `reachable`/`reachableCheckpoints` instead). Valid until the next
// ApiRunBfs/ApiLoadCircuit* call.
ReachabilityView ApiRunBfs(int source, int target);

// Weakly-connected components of the current track (roads treated as
// undirected). Valid until the next ApiRunDfsComponents/ApiLoadCircuit* call.
ComponentsView ApiRunDfsComponents();

// Whether the current track contains a cycle anywhere, directed or
// undirected. Valid until the next ApiRunDfsCycles/ApiLoadCircuit* call.
CycleView ApiRunDfsCycles();

// Non-mandatory vertices the finish cannot be reached from without
// reversing (Project.pdf section 5.1). Valid until the next
// ApiRunDeadEnds/ApiLoadCircuit* call.
DeadEndsView ApiRunDeadEnds();

// Plans the whole remaining lap from the track's Start vertex, through
// every mandatory checkpoint in order, to Finish, under `preset` using
// `algo`. Valid until the next ApiPlanLap/ApiLoadCircuit* call.
RouteView ApiPlanLap(Preset preset, RouteAlgo algo);

// Up to 4 candidate Start->Finish routes (one per preset, deduplicated,
// plus bounded DFS alternatives), each shown under `evalPreset`'s cost so
// the comparison is apples to apples. Valid until the next
// ApiCompareRoutes/ApiLoadCircuit* call.
RouteCompareView ApiCompareRoutes(Preset evalPreset);
