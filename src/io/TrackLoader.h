#pragma once

#include "../model/Graph.h"
#include "../core/ds/DynArray.h"
#include "../core/util/String.h"

namespace io
{
    // A documented fixed detour an opponent uses when `blockedEdgeId` is
    // closed (Project.pdf section 4.4, Opponent 1).
    struct DetourRule
    {
        int blockedEdgeId;
        DynArray<int> viaVertices;
    };

    // One line of a circuit's scripted event stream (Project.pdf section
    // 4.5). `type` is one of "RoadBlock", "RoadClear", "ShortcutToggle",
    // "OilSpill", "SpeedRestriction", "FuelStationToggle", "NitroBooster".
    // `target` is an edge id or vertex id depending on `type`. Interpreted by
    // sim/EventQueue (Phase 2) — the loader only stores the raw fields.
    struct ScriptedEvent
    {
        int triggerTimeMs;
        String type;
        int target;
        int durationMs;
    };

    // Everything TrackLoader produces from one .trk file. Owns `graph` and
    // every DetourRule.
    struct TrackData
    {
        TrackData();
        ~TrackData();

        TrackData(const TrackData&) = delete;
        TrackData& operator=(const TrackData&) = delete;

        model::Graph* graph;
        String name;
        int laps;

        DynArray<int> mainRoute;             // Opponent 1's fixed one-lap vertex sequence
        DynArray<DetourRule*> detours;        // Opponent 1's fixed detours, owned
        DynArray<int> shortcutPreference;     // Opponent 2's preferred shortcut edge ids
        DynArray<ScriptedEvent> events;        // scripted road events for this circuit
    };

    enum class LoadResult
    {
        Ok,
        FileNotFound,
        ParseError,
    };

    // Parses a .trk file into `outData` (see data/circuits/*.trk for the
    // format and io/TrackLoader.cpp's header comment for the grammar).
    // `outData->graph` is freshly allocated on Ok; any previous graph it
    // held is freed first. On ParseError, `errorLine` (1-based) and
    // `errorMessage` describe where parsing failed.
    LoadResult LoadTrackFile(const char* path, TrackData* outData,
        int* errorLine, char* errorMessage, int errorMessageCapacity);
}
