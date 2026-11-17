#include "Kruskal.h"
#include "Sorting.h"
#include "Pseudocode.h"
#include "../core/ds/UnionFind.h"

namespace algo
{
    namespace
    {
        struct WeightOrder
        {
            static int Compare(const CandidateEdge& a, const CandidateEdge& b)
            {
                if (a.weight != b.weight) return (a.weight < b.weight) ? -1 : 1;
                if (a.id != b.id) return (a.id < b.id) ? -1 : 1;
                return 0;
            }

            static int Key(const CandidateEdge& e)
            {
                return e.id;
            }
        };
    }

    MstResult Kruskal(int vertexCount, const CandidateEdge* edges, int edgeCount, AlgoStats* stats, Trace* trace)
    {
        MstResult result;
        if (vertexCount <= 0) return result;

        DynArray<CandidateEdge> sorted;
        for (int i = 0; i < edgeCount; ++i) sorted.Push(edges[i]);
        MergeSort<CandidateEdge, WeightOrder>(sorted.Data(), sorted.Size(), stats);

        UnionFind sets;
        sets.Init(vertexCount);

        for (int i = 0; i < sorted.Size(); ++i)
        {
            const CandidateEdge& e = sorted[i];
            if (stats != nullptr) ++stats->edgesExamined;
            if (trace != nullptr) trace->Record(TraceOp::ExamineEdge, e.id, -1, e.weight, lines::kruskal::kExamine);

            if (e.from < 0 || e.from >= vertexCount || e.to < 0 || e.to >= vertexCount) continue;

            if (sets.Union(e.from, e.to))
            {
                result.acceptedIds.Push(e.id);
                result.totalWeight += e.weight;
                if (trace != nullptr) trace->Record(TraceOp::AcceptEdge, e.id, -1, e.weight, lines::kruskal::kAccept);
            }
            else
            {
                result.rejectedIds.Push(e.id);
                if (trace != nullptr) trace->Record(TraceOp::Reject, e.id, -1, e.weight, lines::kruskal::kReject);
            }
        }

        result.componentCount = sets.ComponentCount();
        return result;
    }
}
