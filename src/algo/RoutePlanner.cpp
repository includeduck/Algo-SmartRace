#include "RoutePlanner.h"
#include "Dijkstra.h"
#include "AStar.h"
#include "Dfs.h"
#include "../model/Rules.h"

#include <cstdio>

namespace algo
{
    PlanResult::~PlanResult()
    {
        for (int i = 0; i < legs.Size(); ++i) delete legs[i];
    }

    PlanResult::PlanResult(PlanResult&& other) noexcept
        : found(other.found), blockedLegIndex(other.blockedLegIndex),
          legs(static_cast<DynArray<LegResult*>&&>(other.legs)),
          vertexPath(static_cast<DynArray<int>&&>(other.vertexPath)),
          edgePath(static_cast<DynArray<int>&&>(other.edgePath)),
          totalCost(other.totalCost), totalDistance(other.totalDistance),
          totalTime(other.totalTime), totalFuel(other.totalFuel)
    {
    }

    PlanResult& PlanResult::operator=(PlanResult&& other) noexcept
    {
        if (this != &other)
        {
            for (int i = 0; i < legs.Size(); ++i) delete legs[i];
            legs = static_cast<DynArray<LegResult*>&&>(other.legs);
            vertexPath = static_cast<DynArray<int>&&>(other.vertexPath);
            edgePath = static_cast<DynArray<int>&&>(other.edgePath);
            found = other.found;
            blockedLegIndex = other.blockedLegIndex;
            totalCost = other.totalCost;
            totalDistance = other.totalDistance;
            totalTime = other.totalTime;
            totalFuel = other.totalFuel;
        }
        return *this;
    }

    RouteCompareResult::~RouteCompareResult()
    {
        for (int i = 0; i < candidates.Size(); ++i) delete candidates[i];
    }

    RouteCompareResult::RouteCompareResult(RouteCompareResult&& other) noexcept
        : candidates(static_cast<DynArray<RouteCandidate*>&&>(other.candidates))
    {
    }

    RouteCompareResult& RouteCompareResult::operator=(RouteCompareResult&& other) noexcept
    {
        if (this != &other)
        {
            for (int i = 0; i < candidates.Size(); ++i) delete candidates[i];
            candidates = static_cast<DynArray<RouteCandidate*>&&>(other.candidates);
        }
        return *this;
    }

    namespace
    {
        // Mandatory checkpoints are few (<=10 by the brief), so a hand-rolled
        // insertion sort by checkpointOrder is simpler here than pulling in
        // the Act 5 sorting module from an Act 2 algorithm.
        void SortCheckpointsByOrder(DynArray<int>* orders, DynArray<int>* vertices)
        {
            for (int i = 1; i < orders->Size(); ++i)
            {
                int keyOrder = (*orders)[i];
                int keyVertex = (*vertices)[i];
                int j = i - 1;
                while (j >= 0 && (*orders)[j] > keyOrder)
                {
                    (*orders)[j + 1] = (*orders)[j];
                    (*vertices)[j + 1] = (*vertices)[j];
                    --j;
                }
                (*orders)[j + 1] = keyOrder;
                (*vertices)[j + 1] = keyVertex;
            }
        }
    }

    PlanResult PlanLap(const model::Graph& graph, int currentVertex, int lastCompletedCheckpoint,
        int finishVertex, const model::Weights& weights, const model::CostReferences& refs,
        RouteAlgo algoChoice, AlgoStats* stats, Trace* trace)
    {
        PlanResult result;
        int n = graph.VertexCount();

        DynArray<int> cpOrders;
        DynArray<int> cpVertices;
        for (int v = 0; v < n; ++v)
        {
            int order = graph.GetVertex(v).checkpointOrder;
            if (order > lastCompletedCheckpoint)
            {
                cpOrders.Push(order);
                cpVertices.Push(v);
            }
        }
        SortCheckpointsByOrder(&cpOrders, &cpVertices);

        DynArray<int> targets;
        for (int i = 0; i < cpVertices.Size(); ++i) targets.Push(cpVertices[i]);
        targets.Push(finishVertex);

        double heuristicScale = 0.0;
        if (algoChoice == RouteAlgo::AStar) heuristicScale = ComputeHeuristicScale(graph, weights, refs);

        int from = currentVertex;
        bool allOk = true;
        double sumCost = 0.0, sumDist = 0.0, sumTime = 0.0, sumFuel = 0.0;

        for (int i = 0; i < targets.Size(); ++i)
        {
            int to = targets[i];

            LegResult* leg = new LegResult();
            leg->fromVertex = from;
            leg->toVertex = to;

            if (algoChoice == RouteAlgo::Dijkstra)
            {
                leg->path = Dijkstra(graph, from, to, weights, refs, stats, trace);
            }
            else
            {
                leg->path = AStar(graph, from, to, weights, refs, heuristicScale, stats, trace);
            }
            leg->found = leg->path.found;
            result.legs.Push(leg);

            if (!leg->found)
            {
                allOk = false;
                result.blockedLegIndex = i;
                break;
            }

            int startIdx = (result.vertexPath.Size() == 0) ? 0 : 1;
            for (int k = startIdx; k < leg->path.vertexPath.Size(); ++k)
            {
                result.vertexPath.Push(leg->path.vertexPath[k]);
            }
            for (int k = 0; k < leg->path.edgePath.Size(); ++k)
            {
                result.edgePath.Push(leg->path.edgePath[k]);
            }

            sumCost += leg->path.totalCost;
            sumDist += leg->path.totalDistance;
            sumTime += leg->path.totalTime;
            sumFuel += leg->path.totalFuel;

            from = to;
        }

        result.found = allOk;
        result.totalCost = sumCost;
        result.totalDistance = sumDist;
        result.totalTime = sumTime;
        result.totalFuel = sumFuel;

        return result;
    }

    namespace
    {
        bool SameEdgeSequence(const DynArray<int>& a, const DynArray<int>& b)
        {
            if (a.Size() != b.Size()) return false;
            for (int i = 0; i < a.Size(); ++i)
            {
                if (a[i] != b[i]) return false;
            }
            return true;
        }

        void EvaluateCandidateUnder(const model::Graph& graph, const model::Weights& w,
            const model::CostReferences& refs, RouteCandidate* candidate)
        {
            double cost = 0.0, dist = 0.0, time = 0.0, fuel = 0.0;
            for (int i = 0; i < candidate->edgePath.Size(); ++i)
            {
                int eid = candidate->edgePath[i];
                cost += model::EdgeCost(graph, eid, w, refs);
                dist += graph.GetEdge(eid).distance;
                time += model::EdgeEffectiveTime(graph, eid);
                fuel += model::EdgeFuelNeed(graph, eid);
            }
            candidate->totalCost = cost;
            candidate->totalDistance = dist;
            candidate->totalTime = time;
            candidate->totalFuel = fuel;
        }

    }

    // Alternative routes come back from DfsAlternativeRoutes as vertex
    // sequences only; re-derive the edge actually used between each
    // consecutive pair (cheapest-by-distance if parallel edges exist).
    // Declared in the header -- see its comment there for who else uses it.
    bool BuildEdgePathFromVertices(const model::Graph& graph, const DynArray<int>& vertexPath,
        DynArray<int>* outEdges)
    {
        for (int i = 0; i + 1 < vertexPath.Size(); ++i)
        {
            int from = vertexPath[i];
            int to = vertexPath[i + 1];

            int bestEdge = -1;
            double bestDistance = -1.0;
            for (const model::AdjNode* node = graph.Neighbors(from); node != nullptr; node = node->next)
            {
                if (node->neighbor != to) continue;
                if (!graph.IsTraversable(node->edgeId)) continue;

                double d = graph.GetEdge(node->edgeId).distance;
                if (bestEdge == -1 || d < bestDistance)
                {
                    bestEdge = node->edgeId;
                    bestDistance = d;
                }
            }

            if (bestEdge == -1) return false;
            outEdges->Push(bestEdge);
        }
        return true;
    }

    RouteCompareResult CompareRoutes(const model::Graph& graph, int source, int target,
        const model::Weights& evalWeights, const model::CostReferences& refs,
        AlgoStats* stats, Trace* trace)
    {
        RouteCompareResult result;

        static const char* const kPresetNames[4] = { "Balanced", "Speed", "Economy", "Safety" };
        static const model::Preset kPresets[4] =
        {
            model::Preset::Balanced, model::Preset::Speed, model::Preset::Economy, model::Preset::Safety,
        };

        DynArray<DynArray<int>*> seenEdgePaths;

        for (int i = 0; i < 4; ++i)
        {
            model::Weights w = model::WeightsForPreset(kPresets[i]);
            PathResult path = Dijkstra(graph, source, target, w, refs, stats, trace);
            if (!path.found) continue;

            bool duplicate = false;
            for (int s = 0; s < seenEdgePaths.Size() && !duplicate; ++s)
            {
                if (SameEdgeSequence(*seenEdgePaths[s], path.edgePath)) duplicate = true;
            }
            if (duplicate) continue;

            DynArray<int>* storedEdges = new DynArray<int>();
            for (int k = 0; k < path.edgePath.Size(); ++k) storedEdges->Push(path.edgePath[k]);
            seenEdgePaths.Push(storedEdges);

            RouteCandidate* candidate = new RouteCandidate();
            candidate->label.Set(kPresetNames[i]);
            candidate->found = true;
            for (int k = 0; k < path.vertexPath.Size(); ++k) candidate->vertexPath.Push(path.vertexPath[k]);
            for (int k = 0; k < path.edgePath.Size(); ++k) candidate->edgePath.Push(path.edgePath[k]);
            EvaluateCandidateUnder(graph, evalWeights, refs, candidate);

            result.candidates.Push(candidate);
        }

        if (result.candidates.Size() > 0)
        {
            DynArray<int> mainRoute;
            for (int k = 0; k < result.candidates[0]->vertexPath.Size(); ++k)
            {
                mainRoute.Push(result.candidates[0]->vertexPath[k]);
            }

            int maxDepth = Rules::kAlternativeRouteMaxDepthMultiplier * graph.VertexCount();
            AlternativeRoutesResult alt = DfsAlternativeRoutes(graph, source, target, mainRoute,
                Rules::kAlternativeRouteCap, maxDepth, stats, trace);

            for (int i = 0; i < alt.routes.Size() && result.candidates.Size() < 4; ++i)
            {
                const DynArray<int>* vertexPath = alt.routes[i];

                DynArray<int> edgePath;
                bool ok = BuildEdgePathFromVertices(graph, *vertexPath, &edgePath);
                if (!ok) continue;

                bool duplicate = false;
                for (int s = 0; s < seenEdgePaths.Size() && !duplicate; ++s)
                {
                    if (SameEdgeSequence(*seenEdgePaths[s], edgePath)) duplicate = true;
                }
                if (duplicate) continue;

                DynArray<int>* storedEdges = new DynArray<int>();
                for (int k = 0; k < edgePath.Size(); ++k) storedEdges->Push(edgePath[k]);
                seenEdgePaths.Push(storedEdges);

                RouteCandidate* candidate = new RouteCandidate();
                char label[32];
                snprintf(label, sizeof(label), "Alternative %d", i + 1);
                candidate->label.Set(label);
                candidate->found = true;
                for (int k = 0; k < vertexPath->Size(); ++k) candidate->vertexPath.Push((*vertexPath)[k]);
                for (int k = 0; k < edgePath.Size(); ++k) candidate->edgePath.Push(edgePath[k]);
                EvaluateCandidateUnder(graph, evalWeights, refs, candidate);

                result.candidates.Push(candidate);
            }
        }

        for (int i = 0; i < seenEdgePaths.Size(); ++i) delete seenEdgePaths[i];

        return result;
    }
}
