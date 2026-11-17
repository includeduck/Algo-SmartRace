#pragma once

#include "../core/ds/DynArray.h"
#include "AlgoStats.h"
#include "Trace.h"

namespace algo
{
    // One undirected candidate road for the spanning tree. `id` is the
    // caller's own identifier (reported back in the result and the trace),
    // and the deterministic tie-break between equal weights.
    struct CandidateEdge
    {
        int id;
        int from;
        int to;
        double weight;
    };

    struct MstResult
    {
        MstResult()
            : totalWeight(0.0), componentCount(0)
        {
        }

        DynArray<int> acceptedIds;   // tree edges, in the order Kruskal accepted them
        DynArray<int> rejectedIds;     // edges that would have closed a cycle
        double totalWeight;
        int componentCount;              // 1 = a spanning tree; > 1 = a spanning FOREST
    };

    // Kruskal's algorithm (Project.pdf section 5.4): sort the candidates by
    // weight (Merge Sort, ties by lower id), then accept each edge whose
    // endpoints are still in different union-find sets. O(E log E) for the
    // sort plus near-constant amortized union-find per edge.
    //
    // If the candidate graph is disconnected the result is a minimum
    // spanning FOREST: componentCount > 1. The brief is explicit that this
    // is not one spanning tree, so the track generator rejects that case
    // instead of building on it.
    MstResult Kruskal(int vertexCount, const CandidateEdge* edges, int edgeCount,
        AlgoStats* stats = nullptr, Trace* trace = nullptr);
}
