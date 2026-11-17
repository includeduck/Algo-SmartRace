#include "Bfs.h"
#include "Pseudocode.h"
#include "../core/ds/Queue.h"

namespace algo
{
    BfsResult Bfs(const model::Graph& graph, int source, int target, AlgoStats* stats, Trace* trace)
    {
        BfsResult result;
        result.source = source;
        result.target = target;

        int n = graph.VertexCount();
        result.distEdges.Resize(n);
        result.parent.Resize(n);
        result.reachable.Resize(n);
        for (int i = 0; i < n; ++i)
        {
            result.distEdges[i] = -1;
            result.parent[i] = -1;
            result.reachable[i] = false;
        }

        if (source < 0 || source >= n) return result;

        Queue<int> frontier;
        result.distEdges[source] = 0;
        result.reachable[source] = true;
        frontier.Enqueue(source);
        if (stats != nullptr) ++stats->verticesVisited;
        if (trace != nullptr) trace->Record(TraceOp::DiscoverVertex, source, -1, 0.0, lines::bfs::kStart);

        while (!frontier.Empty())
        {
            int u = frontier.Dequeue();
            if (trace != nullptr) trace->Record(TraceOp::VisitVertex, u, -1, 0.0, lines::bfs::kDequeue);

            for (const model::AdjNode* node = graph.Neighbors(u); node != nullptr; node = node->next)
            {
                if (stats != nullptr) ++stats->edgesExamined;
                if (trace != nullptr) trace->Record(TraceOp::ExamineEdge, node->edgeId, -1, 0.0, lines::bfs::kExamine);

                if (!graph.IsTraversable(node->edgeId)) continue;

                int v = node->neighbor;
                if (result.reachable[v]) continue;

                result.reachable[v] = true;
                result.distEdges[v] = result.distEdges[u] + 1;
                result.parent[v] = u;
                frontier.Enqueue(v);
                if (stats != nullptr) ++stats->verticesVisited;
                if (trace != nullptr) trace->Record(TraceOp::DiscoverVertex, v, u, result.distEdges[v], lines::bfs::kDiscover);
            }
        }

        if (target >= 0 && target < n)
        {
            result.found = result.reachable[target];
            if (result.found)
            {
                DynArray<int> reversed;
                int cur = target;
                while (cur != -1)
                {
                    reversed.Push(cur);
                    cur = result.parent[cur];
                }
                for (int i = reversed.Size() - 1; i >= 0; --i) result.path.Push(reversed[i]);
            }
        }

        for (int v = 0; v < n; ++v)
        {
            if (!result.reachable[v]) continue;
            int cp = graph.GetVertex(v).checkpointOrder;
            if (cp >= 0) result.reachableCheckpoints.Push(cp);
        }

        return result;
    }
}
