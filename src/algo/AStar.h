#pragma once

#include "PathResult.h"
#include "../model/CostModel.h"
#include "AlgoStats.h"
#include "Trace.h"

namespace algo
{
    // The minimum edge-cost-per-distance ratio over every edge of `graph`
    // under `weights` (computed on the BASE, unblocked graph). This is a
    // valid heuristic scale for AStar() below whenever it is > 0; pass it
    // straight through. If it comes back <= 0 (some edge has zero cost),
    // there is no positive lower-bound ratio, so AStar() must be called
    // with heuristicScale <= 0 (falls back to h = 0, i.e. plain Dijkstra).
    double ComputeHeuristicScale(const model::Graph& graph, const model::Weights& weights,
        const model::CostReferences& refs);

    // A*: Dijkstra guided by h(v) = heuristicScale * euclid(v, target).
    //
    // Admissibility/consistency argument: the track validator requires
    // distance(u,v) >= euclid(u,v) for every edge (so straight-line
    // distance never overestimates a road's true length). Since every cost
    // term is >= 0, cost(u,v) >= weights.wd * distance(u,v) / distanceRef in
    // the worst case is not itself enough -- instead heuristicScale is
    // defined as the minimum cost/euclid ratio over all edges, so for every
    // edge: cost(u,v) >= heuristicScale * euclid(u,v). By the triangle
    // inequality, euclid(u,v) >= euclid(u,target) - euclid(v,target), so
    //   cost(u,v) >= heuristicScale * (euclid(u,target) - euclid(v,target))
    //             =  h(u) - h(v)
    // i.e. h(u) <= cost(u,v) + h(v): h is CONSISTENT (hence admissible) on
    // the base graph. Active effects only ever increase edge cost
    // (blockages remove edges, oil/caps raise cost), so the bound still
    // holds while a race is running. Consistency is exactly why the
    // standard graph-search loop below never needs to reopen a closed
    // vertex — this implementation relies on that and does not reopen.
    PathResult AStar(const model::Graph& graph, int source, int target,
        const model::Weights& weights, const model::CostReferences& refs, double heuristicScale,
        AlgoStats* stats = nullptr, Trace* trace = nullptr);
}
