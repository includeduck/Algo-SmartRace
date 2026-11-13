#pragma once

#include "../model/Graph.h"
#include "../core/ds/DynArray.h"
#include "AlgoStats.h"
#include "Trace.h"

namespace algo
{
    struct DfsTraversalResult
    {
        DynArray<int> discoveryTime;   // -1 if never visited
        DynArray<int> finishTime;        // -1 if never visited
        DynArray<int> parent;             // -1 if none/root
        DynArray<int> order;               // vertices, in first-visit order
    };

    // Iterative DFS from `source` (explicit stack, not recursion — this one
    // also runs on the 100k-vertex benchmark graphs in Experiment 1, where
    // recursion could overflow the call stack). Skips blocked edges.
    DfsTraversalResult DfsTraversal(const model::Graph& graph, int source,
        AlgoStats* stats = nullptr, Trace* trace = nullptr);

    struct DfsCycleResult
    {
        DfsCycleResult()
            : hasCycle(false)
        {
        }

        bool hasCycle;
        DynArray<int> witnessCycle;   // a vertex sequence forming one cycle, if hasCycle
    };

    // Detects a cycle anywhere in the graph, across every component,
    // correctly for a mix of directed and undirected edges: an edge to a
    // vertex currently GRAY (on the recursion stack) is a back edge and
    // means a cycle, UNLESS it is literally the same edge we just arrived
    // by (the trivial "undo the last move" case for an undirected edge).
    DfsCycleResult DfsCycles(const model::Graph& graph, AlgoStats* stats = nullptr, Trace* trace = nullptr);

    struct DfsComponentsResult
    {
        DfsComponentsResult()
            : componentCount(0)
        {
        }

        int componentCount;
        DynArray<int> componentId;   // per vertex, 0-based
    };

    // Weakly-connected components: roads are treated as undirected for
    // this purpose (Project.pdf section 5.1's "connected sections" is a
    // structural property of the track, not of the current block state —
    // live routing's own blocked-edge handling is BFS/Dijkstra/A*'s job).
    DfsComponentsResult DfsComponents(const model::Graph& graph,
        AlgoStats* stats = nullptr, Trace* trace = nullptr);

    struct DfsBridgesResult
    {
        DynArray<int> bridgeEdgeIds;
    };

    // Tarjan bridge-finding, over the graph's UNDIRECTED skeleton (a
    // directed one-way edge is still treated as a structural connection
    // here — see DfsDeadEnds for the actual directed reachability check).
    // Reused by dead-end analysis below and by the generator's obstacle
    // placement pruning (Phase 4: never block a bridge).
    DfsBridgesResult DfsBridges(const model::Graph& graph,
        AlgoStats* stats = nullptr, Trace* trace = nullptr);

    struct DfsDeadEndsResult
    {
        DynArray<int> deadEndVertices;
    };

    // A dead end is a non-mandatory vertex from which the finish cannot be
    // reached without reversing (Project.pdf section 5.1). Computed as the
    // union of: (a) every vertex that cannot reach `finish` at all via the
    // DIRECTED graph (reverse-BFS from finish along Incoming edges — this
    // is what actually matters when one-way roads are present), and (b)
    // for every structural bridge, if the side not containing `finish` has
    // no Start/Checkpoint/Finish vertex on it, every vertex on that side
    // (a spur a car must U-turn out of to continue).
    DfsDeadEndsResult DfsDeadEnds(const model::Graph& graph, int finish,
        AlgoStats* stats = nullptr, Trace* trace = nullptr);

    struct AlternativeRoutesResult
    {
        AlternativeRoutesResult()
            : truncated(false)
        {
        }
        ~AlternativeRoutesResult();
        AlternativeRoutesResult(const AlternativeRoutesResult&) = delete;
        AlternativeRoutesResult& operator=(const AlternativeRoutesResult&) = delete;

        // Hand-written (not `= default`): this struct owns the DynArray<int*>
        // it holds, so move-ASSIGNMENT must free whatever `this` already
        // owned before stealing `other`'s buffer, exactly like the
        // destructor does -- `= default` would just overwrite the member
        // and leak the old routes.
        AlternativeRoutesResult(AlternativeRoutesResult&& other) noexcept;
        AlternativeRoutesResult& operator=(AlternativeRoutesResult&& other) noexcept;

        DynArray<DynArray<int>*> routes;   // owned; each a vertex sequence source..target
        bool truncated;                      // true if maxRoutes or maxDepth was hit
    };

    // Enumerates up to `maxRoutes` simple source->target paths that differ
    // from `mainRoute` by at least one edge, depth-capped at `maxDepth`.
    // Enumerating ALL simple paths is exponential in the worst case
    // (Project.pdf section 5.1's explicit warning), so this is always
    // bounded and reports `truncated` honestly instead of hanging. Uses
    // ordinary recursion — safe here because maxDepth bounds the depth,
    // unlike DfsTraversal above.
    AlternativeRoutesResult DfsAlternativeRoutes(const model::Graph& graph, int source, int target,
        const DynArray<int>& mainRoute, int maxRoutes, int maxDepth,
        AlgoStats* stats = nullptr, Trace* trace = nullptr);
}
