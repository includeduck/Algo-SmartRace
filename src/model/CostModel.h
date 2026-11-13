#pragma once

#include "Graph.h"

namespace model
{
    // Route presets (Project.pdf section 3.3). Distance is an internal
    // preset used by Opponent 3 and by the BFS/A* comparison in Act 2 — it
    // is not offered in the player's preset picker.
    enum class Preset
    {
        Balanced,
        Speed,
        Economy,
        Safety,
        Distance,
    };

    struct Weights
    {
        double wd, wt, wf, wr, wo;
    };

    Weights WeightsForPreset(Preset preset);

    // Reference values used to normalize raw edge attributes before they are
    // combined (Project.pdf section 3.3: distance/time/fuel have different
    // units/scales). Computed once per loaded circuit from the mean base
    // distance/time/fuel of its edges, so normalization does not depend on
    // which preset is active.
    struct CostReferences
    {
        double distanceRef;
        double timeRef;
        double fuelRef;
    };

    CostReferences ComputeReferences(const Graph& graph);

    // Effective travel time for an edge: base speed adjusted for the
    // *effective* difficulty and any active speed cap. Used both for the T
    // term below and by the race engine's own speed math, so "effective
    // time" never has two different definitions.
    double EdgeEffectiveTime(const Graph& graph, int edgeId);

    // The fuel actually needed to cross this edge at its current effective
    // difficulty (Project.pdf section 4.3's fuel-per-edge rule).
    double EdgeFuelNeed(const Graph& graph, int edgeId);

    // The ONE scalar cost function every router optimizes and every display
    // shows (Project.pdf section 3.3: "the cost displayed to the player
    // must be the same cost optimized by the algorithm"). All weights and
    // all normalized terms are >= 0, so the result is >= 0 and Dijkstra
    // applies directly.
    double EdgeCost(const Graph& graph, int edgeId, const Weights& weights, const CostReferences& refs);
}
