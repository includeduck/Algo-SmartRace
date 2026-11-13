#pragma once

#include "PathResult.h"
#include "../model/CostModel.h"
#include "AlgoStats.h"
#include "Trace.h"

namespace algo
{
    // Dijkstra's algorithm over the project's single scalar edge cost
    // (Project.pdf section 5.2). Binary min-heap with lazy decrease-key
    // (push a duplicate on every improvement, skip stale pops via a closed
    // set) rather than an indexed decrease-key — simpler, and the heap's
    // own push/pop counters still give an honest O((V+E) log V) picture for
    // Experiment 2. Blocked edges are excluded. Ties on cost are broken by
    // lower vertex id, so equal-cost paths resolve the same way every run.
    PathResult Dijkstra(const model::Graph& graph, int source, int target,
        const model::Weights& weights, const model::CostReferences& refs,
        AlgoStats* stats = nullptr, Trace* trace = nullptr);
}
