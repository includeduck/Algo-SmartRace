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

        // Drives a route on paper with the engine's fuel rule: every edge
        // burns EdgeFuelTenths, and at an active station on the route
        // (including the first vertex) the racer may buy up to the
        // per-stop limit, capped by the tank. This is the best case for
        // purchases, which is what "can this be driven at all" needs.
        // Returns the index of the first edge the tank cannot cover, or -1.
        int FirstDryEdge(const model::Graph& graph, const DynArray<int>& vertices,
            const DynArray<int>& edges, int startFuel)
        {
            int tank = startFuel;
            for (int i = 0; i < edges.Size(); ++i)
            {
                if (IsActiveStation(graph, vertices[i]))
                {
                    tank += Rules::kMaxStationPurchaseTenths;
                    if (tank > Rules::kMaxFuelTenths) tank = Rules::kMaxFuelTenths;
                }
                tank -= EdgeFuelTenths(graph, edges[i]);
                if (tank < 0) return i;
            }
            return -1;
        }

        struct Insertion
        {
            int at;       // route edge index
            int vertex;     // pickup vertex
            int id;           // item id, orders several insertions at one edge
        };

        void AppendPath(const algo::PathResult& path, DynArray<int>* outVertices, DynArray<int>* outEdges)
        {
            for (int k = 1; k < path.vertexPath.Size(); ++k) outVertices->Push(path.vertexPath[k]);
            for (int k = 0; k < path.edgePath.Size(); ++k) outEdges->Push(path.edgePath[k]);
        }

        // The route with every insertion spliced in: route edge i, when it
        // has insertions p1..pk, becomes u -> p1 -> ... -> pk -> v (least
        // time legs, the same pricing the knapsack uses). `ins` must be
        // sorted by (at, id). *outLastDetourEdge is the index of the last
        // edge belonging to a detour, -1 if there is none.
        bool BuildDetouredRoute(const model::Graph& graph, const DynArray<int>& routeVertices,
            const DynArray<int>& routeEdges, const Insertion* ins, int insCount,
            const model::CostReferences& refs, DynArray<int>* outVertices, DynArray<int>* outEdges,
            int* outLastDetourEdge)
        {
            outVertices->Clear();
            outEdges->Clear();
            *outLastDetourEdge = -1;
            if (routeVertices.Size() == 0) return false;

            model::Weights timeWeights = TimeOnlyWeights();
            outVertices->Push(routeVertices[0]);
            int next = 0;
            for (int i = 0; i < routeEdges.Size(); ++i)
            {
                if (next < insCount && ins[next].at == i)
                {
                    int from = routeVertices[i];
                    while (next < insCount && ins[next].at == i)
                    {
                        algo::PathResult leg = algo::Dijkstra(graph, from, ins[next].vertex, timeWeights, refs);
                        if (!leg.found) return false;
                        AppendPath(leg, outVertices, outEdges);
                        from = ins[next].vertex;
                        ++next;
                    }
                    algo::PathResult back = algo::Dijkstra(graph, from, routeVertices[i + 1], timeWeights, refs);
                    if (!back.found) return false;
                    AppendPath(back, outVertices, outEdges);
                    *outLastDetourEdge = outEdges->Size() - 1;
                }
                else
                {
                    outVertices->Push(routeVertices[i + 1]);
                    outEdges->Push(routeEdges[i]);
                }
            }
            return true;
        }

        // A detoured route is acceptable when it ticks off exactly the
        // checkpoints the plain route does (in order -- ProgressAlong
        // mirrors the engine's counting), and when its fuel works out: the
        // tank never runs dry, or -- if the plain route already runs dry,
        // so the refuel plan must add a station anyway -- it at least lasts
        // until the last detour is over and the racer is back on the route.
        bool DetourFeasible(const model::Graph& graph, const DynArray<int>& routeVertices,
            const DynArray<int>& routeEdges, int fuelTenths, int lastCompleted,
            const Insertion* ins, int insCount, const model::CostReferences& refs)
        {
            DynArray<int> vertices;
            DynArray<int> edges;
            int lastDetourEdge = -1;
            if (!BuildDetouredRoute(graph, routeVertices, routeEdges, ins, insCount, refs,
                &vertices, &edges, &lastDetourEdge))
            {
                return false;
            }
            if (ProgressAlong(graph, vertices, lastCompleted) != ProgressAlong(graph, routeVertices, lastCompleted))
            {
                return false;
            }

            int dry = FirstDryEdge(graph, vertices, edges, fuelTenths);
            if (dry < 0) return true;
            int plainDry = FirstDryEdge(graph, routeVertices, routeEdges, fuelTenths);
            return plainDry >= 0 && dry > lastDetourEdge;
        }

        struct CostThenIdOrder
        {
            static int Compare(const algo::PickupItem& a, const algo::PickupItem& b)
            {
                if (a.cost != b.cost) return (a.cost < b.cost) ? -1 : 1;
                if (a.id != b.id) return (a.id < b.id) ? -1 : 1;
                return 0;
            }

            static int Key(const algo::PickupItem& item)
            {
                return item.id;
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

    bool AppendLapVia(const model::Graph& graph, int fromVertex, int lastCompletedCheckpoint, int via,
        int finishVertex, const model::Weights& weights, const model::CostReferences& refs,
        DynArray<int>* outVertices, DynArray<int>* outEdges)
    {
        algo::PathResult toVia = algo::Dijkstra(graph, fromVertex, via, weights, refs);
        if (!toVia.found) return false;

        int progress = ProgressAlong(graph, toVia.vertexPath, lastCompletedCheckpoint);
        algo::PlanResult onward = algo::PlanLap(graph, via, progress, finishVertex, weights, refs,
            algo::RouteAlgo::Dijkstra);
        if (!onward.found) return false;

        int first = (outVertices->Size() > 0) ? 1 : 0;
        for (int i = first; i < toVia.vertexPath.Size(); ++i) outVertices->Push(toVia.vertexPath[i]);
        for (int i = 1; i < onward.vertexPath.Size(); ++i) outVertices->Push(onward.vertexPath[i]);
        for (int i = 0; i < toVia.edgePath.Size(); ++i) outEdges->Push(toVia.edgePath[i]);
        for (int i = 0; i < onward.edgePath.Size(); ++i) outEdges->Push(onward.edgePath[i]);
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
        const DynArray<int>& routeEdges, int nitroCharges, int fuelTenths, int lastCompletedCheckpoint,
        bool fuelNeeded, DynArray<algo::PickupItem>* outItems, DynArray<PickupInfo>* outInfo)
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
        DynArray<int> insertByVertex;
        insertByVertex.Resize(graph.VertexCount());
        DynArray<int> extraFuelByVertex;
        extraFuelByVertex.Resize(graph.VertexCount());

        for (int p = 0; p < graph.VertexCount(); ++p)
        {
            const model::Vertex& v = graph.GetVertex(p);
            bool isNitro = (v.type == model::VertexType::NitroPickup);
            bool isFuel = (v.type == model::VertexType::FuelStation);
            if (!(isNitro || isFuel) || !v.active) continue;
            if (isFuel && !fuelNeeded) continue;
            if (OnRoute(routeVertices, p)) continue;   // collected on the way anyway

            // Cheapest place to insert the detour along the route, among
            // the places where it can actually be driven.
            bool found = false;
            double best = 0.0;
            int bestAt = -1;
            int bestExtraFuel = 0;
            for (int i = 0; i < routeEdges.Size(); ++i)
            {
                algo::PathResult toPickup = algo::Dijkstra(graph, routeVertices[i], p, timeWeights, refs);
                if (!toPickup.found) continue;
                algo::PathResult back = algo::Dijkstra(graph, p, routeVertices[i + 1], timeWeights, refs);
                if (!back.found) continue;

                double detour = toPickup.totalTime + back.totalTime - model::EdgeEffectiveTime(graph, routeEdges[i]);
                if (found && detour >= best) continue;

                Insertion one;
                one.at = i;
                one.vertex = p;
                one.id = p;
                if (!DetourFeasible(graph, routeVertices, routeEdges, fuelTenths, lastCompletedCheckpoint,
                    &one, 1, refs))
                {
                    continue;
                }

                best = detour;
                bestAt = i;
                bestExtraFuel = RouteFuelTenths(graph, toPickup.edgePath.Data(), toPickup.edgePath.Size())
                    + RouteFuelTenths(graph, back.edgePath.Data(), back.edgePath.Size())
                    - EdgeFuelTenths(graph, routeEdges[i]);
                found = true;
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
            insertByVertex[p] = bestAt;
            extraFuelByVertex[p] = bestExtraFuel;
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
            info.insertAt = insertByVertex[nitroItems[i].vertex];
            info.extraFuelTenths = extraFuelByVertex[nitroItems[i].vertex];
            outInfo->Push(info);
        }
        for (int i = 0; i < fuelItems.Size(); ++i)
        {
            outItems->Push(fuelItems[i]);
            PickupInfo info;
            info.isNitro = false;
            info.detourSeconds = detourByVertex[fuelItems[i].vertex];
            info.insertAt = insertByVertex[fuelItems[i].vertex];
            info.extraFuelTenths = extraFuelByVertex[fuelItems[i].vertex];
            outInfo->Push(info);
        }
    }

    bool PickupSelectionFeasible(const model::Graph& graph, const DynArray<int>& routeVertices,
        const DynArray<int>& routeEdges, int fuelTenths, int lastCompletedCheckpoint,
        const DynArray<algo::PickupItem>& items, const DynArray<PickupInfo>& info,
        const DynArray<int>& selectedIds)
    {
        DynArray<Insertion> ins;
        for (int s = 0; s < selectedIds.Size(); ++s)
        {
            for (int i = 0; i < items.Size(); ++i)
            {
                if (items[i].id != selectedIds[s]) continue;
                Insertion one;
                one.at = info[i].insertAt;
                one.vertex = items[i].vertex;
                one.id = items[i].id;
                ins.Push(one);
            }
        }
        if (ins.Size() == 0) return true;

        // Insertion sort by (at, id): selections hold a handful of items.
        for (int i = 1; i < ins.Size(); ++i)
        {
            Insertion key = ins[i];
            int j = i - 1;
            while (j >= 0 && (ins[j].at > key.at || (ins[j].at == key.at && ins[j].id > key.id)))
            {
                ins[j + 1] = ins[j];
                --j;
            }
            ins[j + 1] = key;
        }

        model::CostReferences refs = model::ComputeReferences(graph);
        return DetourFeasible(graph, routeVertices, routeEdges, fuelTenths, lastCompletedCheckpoint,
            ins.Data(), ins.Size(), refs);
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
