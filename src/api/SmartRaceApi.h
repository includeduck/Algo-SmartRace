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
//     CompareRoutes).
//   v0.3 (Phase 2): ApiRaceStart/Step/Snapshot/Over are now backed by the
//     real deterministic RaceEngine (event queue, effects, fuel/nitro,
//     checkpoint/lap tracking, the 3 opponent strategies, the recovery
//     policy) — their signatures are unchanged from v0.1's stub. Added
//     human commands (ApiCmdChooseEdge/Nitro/Refuel/Autopilot/Preset),
//     ApiRaceResults and ApiRaceLogSince.
//   v0.4 (Phase 3): resource planning (ApiPlanPickups: knapsack DP vs
//     greedy; ApiPlanRefuel: refuelling DP; ApiRecommendedRefuel) and the
//     championship (ApiChampionshipStart/NextRace/GetStatus, ApiStandings
//     by Merge/Quick/Heap Sort, ApiTopK, ApiRaceHistory,
//     ApiTieBreakReason). Racers now genuinely burn fuel and detour to
//     stations when low.
//   v1.0 (Phase 4, FROZEN 29 Nov 2026): the Algorithm Lab (ApiLabRun,
//     ApiLabFrame, ApiLabGraph, ApiLabPseudocode, ApiLabEvents,
//     ApiLabResultText), track validation (ApiValidateTrack; TrackInfo's
//     validatorPassed is now the real validator), the track generator
//     (ApiLabRun(TrackGenerator) + ApiLabGeneratorReport +
//     ApiLabSaveGenerated) and ApiLoadCircuitFile. From here on the API
//     only changes for bug fixes.
// ===========================================================================

#define SMARTRACE_API_VERSION 10

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

// Loads a .trk file directly -- e.g. a generated track saved with
// ApiLabSaveGenerated -- replacing the current circuit, exactly like
// ApiLoadCircuit.
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
// Racing — real as of Phase 2 (deterministic RaceEngine). Starts 1 human
// (autopilot, on the current Settings' preset) + Settings.opponentCount
// opponents cycling through the 3 deterministic strategies.
// ---------------------------------------------------------------------- //

ApiStatus ApiRaceStart();

// Advances the simulation by `ticks` steps of Rules::kSimStepMs each.
void ApiRaceStep(int ticks);

// Valid until the next ApiRaceStep call.
RaceSnapshot ApiRaceSnapshot();

bool ApiRaceOver();

// Human commands. Each returns Illegal (with ApiLastError() set) rather
// than doing something invalid — an illegal edge, insufficient fuel for
// a purchase amount, no nitro charges, etc.
ApiStatus ApiCmdChooseEdge(int edgeId);
ApiStatus ApiCmdNitro();
ApiStatus ApiCmdRefuel(int tenths);
ApiStatus ApiCmdAutopilot(bool on);
ApiStatus ApiCmdPreset(Preset preset);

// Current standings (meaningful mid-race as a live running order too, not
// only once ApiRaceOver()). Valid until the next ApiRaceStep/ApiRaceResults
// call.
ResultsView ApiRaceResults();

// New event-ticker lines since `lastSeenIndex` (pass -1 the first time).
// Valid until the next ApiRaceLogSince/ApiRaceStep call.
LogView ApiRaceLogSince(int lastSeenIndex);

// ---------------------------------------------------------------------- //
// Resource planning (Explorer "Resources" tab, refuel pop-up) — Phase 3
// Both planners work before a race (from the Start, full tank, whole
// race) and during one (from the human's current position, fuel and
// remaining laps).
// ---------------------------------------------------------------------- //

// Optional pickups along the human's route under `preset`, solved both by
// the knapsack DP and by the ratio greedy within `budgetTenthsSec` of
// extra detour time (pass Rules' default, 150, if unsure). Valid until the
// next ApiPlanPickups/ApiLoadCircuit* call.
PickupView ApiPlanPickups(Preset preset, int budgetTenthsSec);

// The refuelling DP over the rest of the race on the human's route under
// `preset`: where to stop and how much to buy, or infeasible. Valid until
// the next ApiPlanRefuel/ApiLoadCircuit* call.
RefuelView ApiPlanRefuel(Preset preset);

// What the refuelling DP recommends the human buy at the station they are
// stopped at right now (tenths; 0 if not at an active station). Pre-fill
// the refuel pop-up's slider with this.
int ApiRecommendedRefuel();

// ---------------------------------------------------------------------- //
// Championship (Screen 5) — Phase 3
// Flow: ApiChampionshipStart() -> [ApiRaceStart(), step until
// ApiRaceOver(), ApiChampionshipNextRace()] x 3. Circuits run in the fixed
// order 0, 1, 2; NextRace records the finished race and loads the next
// circuit.
// ---------------------------------------------------------------------- //

// Resets standings for 1 human + Settings.opponentCount opponents and
// loads circuit 0.
ApiStatus ApiChampionshipStart();

// Records the race that just finished (Illegal if none is over yet), then
// loads the next circuit unless the championship is complete.
ApiStatus ApiChampionshipNextRace();

ChampionshipStatus ApiChampionshipGetStatus();

// Standings sorted by `method`, with that sort's counters. Valid until the
// next ApiStandings/ApiChampionship* call.
StandingsView ApiStandings(SortMethod method);

// The best k racers via the size-k heap. Valid until the next ApiTopK/
// ApiChampionship* call.
TopKView ApiTopK(int k);

// Results of championship race `raceIndex` (0-based). Valid until the next
// ApiRaceHistory/ApiChampionship* call.
ResultsView ApiRaceHistory(int raceIndex);

// Why racerA and racerB are ordered as they are in the standings.
TieBreak ApiTieBreakReason(int racerA, int racerB);

// ---------------------------------------------------------------------- //
// Track validation — Phase 4
// ---------------------------------------------------------------------- //

// Every playability check on the loaded circuit (computed when it was
// loaded). Valid until the next ApiLoadCircuit* call.
ValidationView ApiValidateTrack();

// ---------------------------------------------------------------------- //
// Algorithm Lab (Screen 4) — Phase 4
// ApiLabRun runs the SAME function gameplay uses, once, with a recorder
// attached; ApiLabFrame then replays any step. Graph algorithms, Kruskal,
// the knapsack/refuel planners, the event queue and the sorts run on the
// CURRENT data: the loaded circuit (with any live race effects), and the
// championship standings when one is running.
// ---------------------------------------------------------------------- //

LabInput ApiLabDefaultInput();

// Runs `algo` and records every step. Needs a loaded circuit, except for
// the sorts, the brief's knapsack example and the generator.
ApiStatus ApiLabRun(LabAlgo algo, const LabInput& input);

int ApiLabStepCount();

// State after step `step` (-1 = initial, clamped to the last step). Seeking
// forward is incremental; seeking backward replays from the start.
LabFrame ApiLabFrame(int step);

// The graph a Graph-kind frame indexes into: the loaded circuit, or for the
// generator its candidate-road graph (every candidate, kept or not). Valid
// until the next ApiLabRun/ApiLoadCircuit* call.
GraphView ApiLabGraph();

PseudocodeView ApiLabPseudocode(LabAlgo algo);

// A one-line summary of the last run's result (path cost, items chosen,
// tree weight, accepted attempt, ...).
const char* ApiLabResultText();

// The events the EventQueue lab scheduled. Valid until the next ApiLabRun.
ArrayView<LabEventView> ApiLabEvents();

// Every attempt of the last generator run (accepted or rejected, and why).
GeneratorReportView ApiLabGeneratorReport();

// Writes the last generated track as a .trk file; ApiLoadCircuitFile(path)
// then makes it the playable circuit.
ApiStatus ApiLabSaveGenerated(const char* path);

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
