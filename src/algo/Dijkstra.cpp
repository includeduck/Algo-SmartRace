#include "Dijkstra.h"
#include "Pseudocode.h"
#include "../core/ds/MinHeap.h"

namespace algo
{
    namespace
    {
        struct HeapItem
        {
            double key;
            int vertex;
        };

        struct HeapItemLess
        {
            static bool Less(const HeapItem& a, const HeapItem& b)
            {
                if (a.key != b.key) return a.key < b.key;
                return a.vertex < b.vertex;   // deterministic tie-break
            }
        };

        const double kInfinity = 1.0e18;
    }

    PathResult Dijkstra(const model::Graph& graph, int source, int target,
        const model::Weights& weights, const model::CostReferences& refs,
        AlgoStats* stats, Trace* trace)
    {
        PathResult result;
        result.source = source;
        result.target = target;

        int n = graph.VertexCount();
        if (source < 0 || source >= n || target < 0 || target >= n) return result;

        DynArray<double> dist;
        DynArray<int> parent;
        DynArray<int> parentEdge;
        DynArray<bool> closed;
        dist.Resize(n);
        parent.Resize(n);
        parentEdge.Resize(n);
        closed.Resize(n);
        for (int i = 0; i < n; ++i)
        {
            dist[i] = kInfinity;
            parent[i] = -1;
            parentEdge[i] = -1;
            closed[i] = false;
        }

        MinHeap<HeapItem, HeapItemLess> heap;
        dist[source] = 0.0;
        HeapItem startItem = { 0.0, source };
        heap.Push(startItem);
        if (stats != nullptr) ++stats->heapPushes;
        if (trace != nullptr) trace->Record(TraceOp::DiscoverVertex, source, -1, 0.0, lines::shortest::kStart);

        while (!heap.Empty())
        {
            HeapItem top = heap.Pop();
            if (stats != nullptr) ++stats->heapPops;
            if (trace != nullptr) trace->Record(TraceOp::PopHeap, top.vertex, -1, top.key, lines::shortest::kPop);

            int u = top.vertex;
            if (closed[u]) continue;   // stale lazy-deleted entry
            closed[u] = true;
            if (stats != nullptr) ++stats->verticesVisited;
            if (trace != nullptr) trace->Record(TraceOp::VisitVertex, u, -1, dist[u], lines::shortest::kClose);

            if (u == target) break;

            for (const model::AdjNode* node = graph.Neighbors(u); node != nullptr; node = node->next)
            {
                if (stats != nullptr) ++stats->edgesExamined;
                if (trace != nullptr) trace->Record(TraceOp::ExamineEdge, node->edgeId, -1, 0.0, lines::shortest::kExamine);

                if (!graph.IsTraversable(node->edgeId)) continue;

                int v = node->neighbor;
                if (closed[v]) continue;

                double edgeCost = model::EdgeCost(graph, node->edgeId, weights, refs);
                double candidate = dist[u] + edgeCost;

                if (candidate < dist[v])
                {
                    dist[v] = candidate;
                    parent[v] = u;
                    parentEdge[v] = node->edgeId;
                    if (stats != nullptr) ++stats->edgesRelaxed;
                    if (trace != nullptr) trace->Record(TraceOp::RelaxEdge, node->edgeId, v, candidate, lines::shortest::kRelax);

                    HeapItem item = { candidate, v };
                    heap.Push(item);
                    if (stats != nullptr) ++stats->heapPushes;
                    if (trace != nullptr) trace->Record(TraceOp::PushHeap, v, -1, candidate, lines::shortest::kPush);
                }
            }
        }

        if (closed[target] && dist[target] < kInfinity)
        {
            ReconstructPath(graph, target, dist[target], parent, parentEdge, &result);
        }

        return result;
    }
}
