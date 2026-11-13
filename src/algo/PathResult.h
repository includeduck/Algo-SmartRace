#pragma once

#include "../model/Graph.h"
#include "../core/ds/DynArray.h"

namespace algo
{
    // The result shape shared by Dijkstra and A* (Project.pdf section
    // 5.2): the brief requires the exact scalar cost shown to the player
    // to be the same cost the algorithm optimized, so `totalCost` is that
    // one number, alongside the raw distance/time/fuel totals for display.
    struct PathResult
    {
        PathResult()
            : found(false), source(-1), target(-1),
              totalCost(0.0), totalDistance(0.0), totalTime(0.0), totalFuel(0.0)
        {
        }

        bool found;
        int source;
        int target;

        double totalCost;         // the exact scalar cost the search minimized
        double totalDistance;
        double totalTime;
        double totalFuel;

        DynArray<int> vertexPath;   // source..target, only set if found
        DynArray<int> edgePath;       // edges used, size == vertexPath.Size() - 1
    };

    // Shared by Dijkstra and A*: walks `parent`/`parentEdge` back from
    // `target` to its root, fills vertexPath/edgePath in forward order, and
    // sums up distance/time/fuel over the reconstructed edges.
    void ReconstructPath(const model::Graph& graph, int target, double totalCost,
        const DynArray<int>& parent, const DynArray<int>& parentEdge, PathResult* result);
}
