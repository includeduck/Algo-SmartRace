#include "AStar.h"
#include "../core/ds/MinHeap.h"
#include "../core/util/MathUtil.h"

namespace algo
{
    namespace
    {
        struct HeapItem
        {
            double f;
            int vertex;
        };

        struct HeapItemLess
        {
            static bool Less(const HeapItem& a, const HeapItem& b)
            {
                if (a.f != b.f) return a.f < b.f;
                return a.vertex < b.vertex;   // deterministic tie-break
            }
        };

        const double kInfinity = 1.0e18;

        double Heuristic(const model::Graph& graph, int v, int target, double scale)
        {
            if (scale <= 0.0) return 0.0;
            const model::Vertex& a = graph.GetVertex(v);
            const model::Vertex& b = graph.GetVertex(target);
            return scale * MathUtil::Dist2D(a.x, a.y, b.x, b.y);
        }
    }

    double ComputeHeuristicScale(const model::Graph& graph, const model::Weights& weights,
        const model::CostReferences& refs)
    {
        double minRatio = -1.0;
        int m = graph.EdgeCount();

        for (int i = 0; i < m; ++i)
        {
            const model::Edge& e = graph.GetEdge(i);
            const model::Vertex& from = graph.GetVertex(e.from);
            const model::Vertex& to = graph.GetVertex(e.to);
            double euclid = MathUtil::Dist2D(from.x, from.y, to.x, to.y);
            if (euclid <= 0.0) continue;

            double cost = model::EdgeCost(graph, i, weights, refs);
            double ratio = cost / euclid;
            if (minRatio < 0.0 || ratio < minRatio) minRatio = ratio;
        }

        return minRatio;
    }

    PathResult AStar(const model::Graph& graph, int source, int target,
        const model::Weights& weights, const model::CostReferences& refs, double heuristicScale,
        AlgoStats* stats, Trace* trace)
    {
        PathResult result;
        result.source = source;
        result.target = target;

        int n = graph.VertexCount();
        if (source < 0 || source >= n || target < 0 || target >= n) return result;

        DynArray<double> g;
        DynArray<int> parent;
        DynArray<int> parentEdge;
        DynArray<bool> closed;
        g.Resize(n);
        parent.Resize(n);
        parentEdge.Resize(n);
        closed.Resize(n);
        for (int i = 0; i < n; ++i)
        {
            g[i] = kInfinity;
            parent[i] = -1;
            parentEdge[i] = -1;
            closed[i] = false;
        }

        MinHeap<HeapItem, HeapItemLess> heap;
        g[source] = 0.0;
        HeapItem startItem = { Heuristic(graph, source, target, heuristicScale), source };
        heap.Push(startItem);
        if (stats != nullptr) ++stats->heapPushes;

        while (!heap.Empty())
        {
            HeapItem top = heap.Pop();
            if (stats != nullptr) ++stats->heapPops;

            int u = top.vertex;
            if (closed[u]) continue;
            closed[u] = true;   // consistent heuristic: never reopened
            if (stats != nullptr) ++stats->verticesVisited;
            if (trace != nullptr) trace->Record(TraceOp::VisitVertex, u);

            if (u == target) break;

            for (const model::AdjNode* node = graph.Neighbors(u); node != nullptr; node = node->next)
            {
                if (stats != nullptr) ++stats->edgesExamined;
                if (trace != nullptr) trace->Record(TraceOp::ExamineEdge, node->edgeId);

                if (!graph.IsTraversable(node->edgeId)) continue;

                int v = node->neighbor;
                if (closed[v]) continue;

                double edgeCost = model::EdgeCost(graph, node->edgeId, weights, refs);
                double candidate = g[u] + edgeCost;

                if (candidate < g[v])
                {
                    g[v] = candidate;
                    parent[v] = u;
                    parentEdge[v] = node->edgeId;
                    if (stats != nullptr) ++stats->edgesRelaxed;
                    if (trace != nullptr) trace->Record(TraceOp::RelaxEdge, node->edgeId, -1, candidate);

                    double f = candidate + Heuristic(graph, v, target, heuristicScale);
                    HeapItem item = { f, v };
                    heap.Push(item);
                    if (stats != nullptr) ++stats->heapPushes;
                }
            }
        }

        if (closed[target] && g[target] < kInfinity)
        {
            ReconstructPath(graph, target, g[target], parent, parentEdge, &result);
        }

        return result;
    }
}
