#include "ResourcePlanner.h"
#include "../algo/Dijkstra.h"
#include "../algo/RoutePlanner.h"
#include "../algo/Sorting.h"
#include "../model/Rules.h"

namespace sim
{
    namespace
    {
        bool IsActiveStation(const model::Graph& graph, int vertex)
        {
            const model::Vertex& v = graph.GetVertex(vertex);
            return v.type == model::VertexType::FuelStation && v.active;
        }

        bool OnRoute(const DynArray<int>& route, int vertex)
        {
            for (int i = 0; i < route.Size(); ++i)
            {
                if (route[i] == vertex) return true;
            }
            return false;
        }

        bool Adjacent(const model::Graph& graph, int from, int to)
        {
            for (const model::AdjNode* node = graph.Neighbors(from); node != nullptr; node = node->next)
            {
                if (node->neighbor == to && graph.IsTraversable(node->edgeId)) return true;
            }
            return false;
        }

        // Pure travel time: only the T term of the cost model is weighted,
        // so a least-cost path is a least-time path and PathResult's
        // totalTime is that time in seconds.
        model::Weights TimeOnlyWeights()
        {
            model::Weights w = { 0.0, 1.0, 0.0, 0.0, 0.0 };
            return w;
        }

        // Checkpoints a path ticks off in order, starting after
        // `lastCompleted` -- so a detour that happens to pass the next
        // checkpoint does not send the racer back to it afterwards.
        int ProgressAlong(const model::Graph& graph, const DynArray<int>& path, int lastCompleted)
        {
            for (int i = 0; i < path.Size(); ++i)
            {
                if (graph.GetVertex(path[i]).checkpointOrder == lastCompleted + 1) ++lastCompleted;
            }
            return lastCompleted;
        }

        struct CostThenIdOrder
        {
            static int Compare(const algo::PickupItem& a, const algo::PickupItem& b)
            {
                if (a.cost != b.cost) return (a.cost < b.cost) ? -1 : 1;
                if (a.id != b.id) return (a.id < b.id) ? -1 : 1;
                return 0;
            }
        };
    }

    int EdgeFuelTenths(const model::Graph& graph, int edgeId)
    {
        return static_cast<int>(model::EdgeFuelNeed(graph, edgeId) * 10.0 + 0.5);
    }

    int RouteFuelTenths(const model::Graph& graph, const int* edges, int count)
    {
        int total = 0;
        for (int i = 0; i < count; ++i) total += EdgeFuelTenths(graph, edges[i]);
        return total;
    }

    bool BuildRaceRoute(const model::Graph& graph, int currentVertex, int lastCompletedCheckpoint,
        int extraFullLaps, int finishVertex, const model::Weights& weights, const model::CostReferences& refs,
        DynArray<int>* outVertices, DynArray<int>* outEdges)
    {
        outVertices->Clear();
        outEdges->Clear();

        algo::PlanResult first = algo::PlanLap(graph, currentVertex, lastCompletedCheckpoint, finishVertex,
            weights, refs, algo::RouteAlgo::Dijkstra);
        if (!first.found) return false;

        for (int i = 0; i < first.vertexPath.Size(); ++i) outVertices->Push(first.vertexPath[i]);
        for (int i = 0; i < first.edgePath.Size(); ++i) outEdges->Push(first.edgePath[i]);

        if (extraFullLaps <= 0) return true;

        // Every further lap is the same plan from the finish, so plan it
        // once and repeat it.
        algo::PlanResult lap = algo::PlanLap(graph, finishVertex, 0, finishVertex, weights, refs,
            algo::RouteAlgo::Dijkstra);
        if (!lap.found) return false;

        for (int l = 0; l < extraFullLaps; ++l)
        {
            for (int i = 1; i < lap.vertexPath.Size(); ++i) outVertices->Push(lap.vertexPath[i]);
            for (int i = 0; i < lap.edgePath.Size(); ++i) outEdges->Push(lap.edgePath[i]);
        }
        return true;
    }

    void BuildRefuelProblem(const model::Graph& graph, const DynArray<int>& vertices,
        const DynArray<int>& edges, int startFuel, algo::RefuelProblem* out)
    {
        out->legFuel.Clear();
        out->canBuyAt.Clear();
        out->stopVertex.Clear();
        out->startFuel = startFuel;
        out->capacity = Rules::kMaxFuelTenths;
        out->maxPurchase = Rules::kMaxStationPurchaseTenths;
        out->stopTimeMs = Rules::kStationStopTimeMs;

        if (vertices.Size() == 0) return;
        out->stopVertex.Push(vertices[0]);
        if (edges.Size() == 0) return;   // already at the destination: zero legs

        out->canBuyAt.Push(IsActiveStation(graph, vertices[0]));

        int legFuel = 0;
        for (int i = 0; i < edges.Size(); ++i)
        {
            legFuel += EdgeFuelTenths(graph, edges[i]);
            int v = vertices[i + 1];
            bool isLast = (i == edges.Size() - 1);

            if (isLast)
            {
                out->legFuel.Push(legFuel);
                out->stopVertex.Push(v);
            }
            else if (graph.GetVertex(v).type == model::VertexType::FuelStation)
            {
                // A deactivated station is still a stop on the route, but
                // nothing can be bought there (canBuyAt = false).
                out->legFuel.Push(legFuel);
                out->stopVertex.Push(v);
                out->canBuyAt.Push(graph.GetVertex(v).active);
                legFuel = 0;
            }
        }
    }

    void BuildPickupItems(const model::Graph& graph, const DynArray<int>& routeVertices,
        const DynArray<int>& routeEdges, int nitroCharges, bool fuelNeeded,
        DynArray<algo::PickupItem>* outItems, DynArray<PickupInfo>* outInfo)
    {
        outItems->Clear();
        outInfo->Clear();
        if (routeEdges.Size() == 0) return;

        model::Weights timeWeights = TimeOnlyWeights();
        model::CostReferences refs = model::ComputeReferences(graph);

        DynArray<algo::PickupItem> nitroItems;
        DynArray<algo::PickupItem> fuelItems;
        DynArray<double> detourByVertex;
        detourByVertex.Resize(graph.VertexCount());

        for (int p = 0; p < graph.VertexCount(); ++p)
        {
            const model::Vertex& v = graph.GetVertex(p);
            bool isNitro = (v.type == model::VertexType::NitroPickup);
            bool isFuel = (v.type == model::VertexType::FuelStation);
            if (!(isNitro || isFuel) || !v.active) continue;
            if (isFuel && !fuelNeeded) continue;
            if (OnRoute(routeVertices, p)) continue;   // collected on the way anyway

            // Cheapest place to insert the detour along the route.
            bool found = false;
            double best = 0.0;
            for (int i = 0; i < routeEdges.Size(); ++i)
            {
                algo::PathResult toPickup = algo::Dijkstra(graph, routeVertices[i], p, timeWeights, refs);
                if (!toPickup.found) continue;
                algo::PathResult back = algo::Dijkstra(graph, p, routeVertices[i + 1], timeWeights, refs);
                if (!back.found) continue;

                double detour = toPickup.totalTime + back.totalTime - model::EdgeEffectiveTime(graph, routeEdges[i]);
                if (!found || detour < best)
                {
                    best = detour;
                    found = true;
                }
            }
            if (!found) continue;
            if (best < 0.0) best = 0.0;   // can only happen with a slow route edge; a detour never "saves" time here

            algo::PickupItem item;
            item.id = p;
            item.vertex = p;
            item.cost = static_cast<int>(best * 10.0 + 0.5);
            item.benefit = isNitro ? Rules::kNitroPickupBenefitTenthsSec : Rules::kFuelPickupBenefitTenthsSec;
            if (isFuel) item.cost += Rules::kStationStopTimeMs / 100;   // ms -> tenths of a second

            detourByVertex[p] = best;
            if (isNitro)
            {
                nitroItems.Push(item);
            }
            else
            {
                fuelItems.Push(item);
            }
        }

        // A charge beyond capacity is worth nothing: offer only as many
        // nitro pickups as the racer could still use, cheapest first.
        int usable = Rules::kMaxNitroCharges - nitroCharges;
        if (usable < 0) usable = 0;
        algo::MergeSort<algo::PickupItem, CostThenIdOrder>(nitroItems.Data(), nitroItems.Size());

        for (int i = 0; i < nitroItems.Size() && i < usable; ++i)
        {
            outItems->Push(nitroItems[i]);
            PickupInfo info;
            info.isNitro = true;
            info.detourSeconds = detourByVertex[nitroItems[i].vertex];
            outInfo->Push(info);
        }
        for (int i = 0; i < fuelItems.Size(); ++i)
        {
            outItems->Push(fuelItems[i]);
            PickupInfo info;
            info.isNitro = false;
            info.detourSeconds = detourByVertex[fuelItems[i].vertex];
            outInfo->Push(info);
        }
    }

    bool PlanStationDetour(const model::Graph& graph, int currentVertex, int fuelTenths,
        int lastCompletedCheckpoint, int finishVertex, const model::Weights& weights,
        const model::CostReferences& refs, StationChoice choice,
        const int* referenceRoute, int referenceLen,
        DynArray<int>* outVertices, DynArray<int>* outEdges, int* outStation)
    {
        outVertices->Clear();
        outEdges->Clear();
        *outStation = -1;

        int bestStation = -1;
        int bestAttach = 0x7fffffff;
        double bestCost = 0.0;

        for (int s = 0; s < graph.VertexCount(); ++s)
        {
            if (s == currentVertex || !IsActiveStation(graph, s)) continue;

            algo::PathResult toStation = algo::Dijkstra(graph, currentVertex, s, weights, refs);
            if (!toStation.found) continue;
            if (RouteFuelTenths(graph, toStation.edgePath.Data(), toStation.edgePath.Size()) > fuelTenths) continue;

            int progress = ProgressAlong(graph, toStation.vertexPath, lastCompletedCheckpoint);
            algo::PlanResult onward = algo::PlanLap(graph, s, progress, finishVertex, weights, refs,
                algo::RouteAlgo::Dijkstra);
            if (!onward.found) continue;

            int attach = 0x7fffffff;
            if (choice == StationChoice::FirstOnRoute && referenceRoute != nullptr)
            {
                for (int i = 0; i < referenceLen; ++i)
                {
                    if (Adjacent(graph, referenceRoute[i], s))
                    {
                        attach = i;
                        break;
                    }
                }
            }

            bool better = false;
            if (bestStation == -1)
            {
                better = true;
            }
            else if (attach != bestAttach)
            {
                better = (attach < bestAttach);
            }
            else
            {
                better = (toStation.totalCost < bestCost);
            }

            if (better)
            {
                bestStation = s;
                bestAttach = attach;
                bestCost = toStation.totalCost;
            }
        }

        if (bestStation == -1) return false;

        algo::PathResult toStation = algo::Dijkstra(graph, currentVertex, bestStation, weights, refs);
        int progress = ProgressAlong(graph, toStation.vertexPath, lastCompletedCheckpoint);
        algo::PlanResult onward = algo::PlanLap(graph, bestStation, progress, finishVertex, weights, refs,
            algo::RouteAlgo::Dijkstra);

        for (int i = 0; i < toStation.vertexPath.Size(); ++i) outVertices->Push(toStation.vertexPath[i]);
        for (int i = 1; i < onward.vertexPath.Size(); ++i) outVertices->Push(onward.vertexPath[i]);
        for (int i = 0; i < toStation.edgePath.Size(); ++i) outEdges->Push(toStation.edgePath[i]);
        for (int i = 0; i < onward.edgePath.Size(); ++i) outEdges->Push(onward.edgePath[i]);

        *outStation = bestStation;
        return true;
    }
}
