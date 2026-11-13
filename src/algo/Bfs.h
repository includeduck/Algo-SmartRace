#pragma once

#include "../model/Graph.h"
#include "../core/ds/DynArray.h"
#include "AlgoStats.h"
#include "Trace.h"

namespace algo
{
    struct BfsResult
    {
        BfsResult()
            : found(false), source(-1), target(-1)
        {
        }

        bool found;    // meaningful only when target >= 0
        int source;
        int target;     // -1 means "full reachability, no specific target"

        DynArray<int> distEdges;      // edge-count distance from source, -1 if unreached
        DynArray<int> parent;           // BFS-tree parent, -1 if none/root
        DynArray<bool> reachable;        // reachable[v]

        DynArray<int> path;                    // source..target, only set if found
        DynArray<int> reachableCheckpoints;    // mandatory checkpoint ORDER numbers reachable
    };

    // Breadth-first search from `source` (Project.pdf section 5.1). If
    // `target` >= 0, also reconstructs the path with the minimum EDGE
    // COUNT to it — not minimum distance/time/cost (that is Dijkstra/A*'s
    // job; see the brief's own S-T worked example). Skips blocked edges.
    // An unreachable target returns found=false; it never loops, even on a
    // disconnected or single-vertex graph.
    BfsResult Bfs(const model::Graph& graph, int source, int target,
        AlgoStats* stats = nullptr, Trace* trace = nullptr);
}
