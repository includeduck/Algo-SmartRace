#include "PathResult.h"
#include "../model/CostModel.h"

namespace algo
{
    void ReconstructPath(const model::Graph& graph, int target, double totalCost,
        const DynArray<int>& parent, const DynArray<int>& parentEdge, PathResult* result)
    {
        result->found = true;
        result->totalCost = totalCost;

        DynArray<int> revVerts;
        DynArray<int> revEdges;
        int cur = target;
        while (cur != -1)
        {
            revVerts.Push(cur);
            if (parentEdge[cur] != -1) revEdges.Push(parentEdge[cur]);
            cur = parent[cur];
        }

        for (int i = revVerts.Size() - 1; i >= 0; --i) result->vertexPath.Push(revVerts[i]);
        for (int i = revEdges.Size() - 1; i >= 0; --i) result->edgePath.Push(revEdges[i]);

        double totalDistance = 0.0;
        double totalTime = 0.0;
        double totalFuel = 0.0;
        for (int i = 0; i < result->edgePath.Size(); ++i)
        {
            int eid = result->edgePath[i];
            totalDistance += graph.GetEdge(eid).distance;
            totalTime += model::EdgeEffectiveTime(graph, eid);
            totalFuel += model::EdgeFuelNeed(graph, eid);
        }
        result->totalDistance = totalDistance;
        result->totalTime = totalTime;
        result->totalFuel = totalFuel;
    }
}
