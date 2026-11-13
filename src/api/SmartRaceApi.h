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
//     end to end immediately. Track Explorer queries (BFS/DFS/Dijkstra/A*),
//     the real race engine, resource planning, championship and the
//     Algorithm Lab are added in later phases as pure ADDITIONS to this
//     header — existing signatures below do not change after a phase ships
//     them for real. API freeze: 29 Nov 2026.
// ===========================================================================

#define SMARTRACE_API_VERSION 1

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
