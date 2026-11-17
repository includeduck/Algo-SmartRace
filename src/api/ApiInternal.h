#pragma once

#include "SmartRaceApi.h"
#include "../model/Graph.h"
#include "../model/CostModel.h"
#include "../io/TrackLoader.h"
#include "../sim/ChampionshipManager.h"
#include "../core/ds/DynArray.h"

// Shared between SmartRaceApi.cpp and Lab.cpp only. NOT part of the
// GUI-facing API: the GUI includes SmartRaceApi.h and nothing else.
namespace api_internal
{
    bool CircuitLoaded();
    model::Graph* CurrentGraph();
    const io::TrackData& CurrentTrack();
    int StartVertex();
    int FinishVertex();
    const model::CostReferences& CostRefs();
    int Laps();

    bool ChampionshipActive();
    const sim::ChampionshipManager& Championship();

    void SetError(const char* message);
    model::Preset ToModelPreset(Preset preset);
    void BuildGraphViews(const model::Graph& graph, DynArray<VertexView>* vertices, DynArray<EdgeView>* edges);

    // Called by ApiLoadCircuit* so the Lab drops results that index into a
    // graph that is about to change. Keeps the last generated track, so it
    // can still be saved after loading another circuit.
    void ResetLab();

    // ResetLab plus the generated track and generator report -- called by
    // ApiInit/ApiShutdown, which reset everything.
    void ShutdownLab();
}
