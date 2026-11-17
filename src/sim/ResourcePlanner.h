#pragma once

#include "../model/Graph.h"
#include "../model/CostModel.h"
#include "../algo/Knapsack.h"
#include "../algo/RefuelDp.h"
#include "../core/ds/DynArray.h"

// Act 3 integration (Project.pdf section 5.3): turns the live track and a
// racer's planned route into inputs for the refuelling DP and the pickup
// knapsack, and plans the detour to a fuel station when a racer runs low.
// Every quantity here is derived from the same graph, cost model and fuel
// rule the race engine uses, so a recommendation is always something the
// engine can actually execute (the brief's "must match the race engine's
// rules" requirement).
namespace sim
{
    // Fuel (tenths) to drive `edgeId` at its current effective difficulty:
    // fuelCost x (1 + 0.1 x (effDifficulty - 1)), rounded to a tenth. The
    // race engine charges exactly this amount over the length of the edge.
    int EdgeFuelTenths(const model::Graph& graph, int edgeId);

    int RouteFuelTenths(const model::Graph& graph, const int* edges, int count);

    // The route for the REST OF THE RACE: from `currentVertex` through the
    // checkpoints after `lastCompletedCheckpoint` to the finish, then
    // `extraFullLaps` more complete laps from the finish. Each lap is
    // planned with algo::PlanLap under `weights`. Returns false if any leg
    // is unreachable right now.
    bool BuildRaceRoute(const model::Graph& graph, int currentVertex, int lastCompletedCheckpoint,
        int extraFullLaps, int finishVertex, const model::Weights& weights, const model::CostReferences& refs,
        DynArray<int>* outVertices, DynArray<int>* outEdges);

    // APPENDS to out*: a route from `fromVertex` to `via` (one Dijkstra
    // search), then on through every checkpoint after the ones completed by
    // then, to the finish. When out* already holds a route ending at
    // `fromVertex`, the duplicate join vertex is not repeated, so laps can
    // be chained. Returns false if any part is unreachable.
    bool AppendLapVia(const model::Graph& graph, int fromVertex, int lastCompletedCheckpoint, int via,
        int finishVertex, const model::Weights& weights, const model::CostReferences& refs,
        DynArray<int>* outVertices, DynArray<int>* outEdges);

    // Cuts a route into refuelling-DP stops: stop 0 is the route's first
    // vertex (a purchase is possible there only if it is an active station),
    // then every fuel station the route passes, in order (a station passed
    // on two laps is two stops), then the route's last vertex.
    void BuildRefuelProblem(const model::Graph& graph, const DynArray<int>& vertices,
        const DynArray<int>& edges, int startFuel, algo::RefuelProblem* out);

    struct PickupInfo
    {
        bool isNitro;           // false = fuel station
        double detourSeconds;     // extra driving time the detour adds
    };

    // Optional pickups as knapsack items. A candidate is any active nitro
    // pickup or active fuel station NOT already on `routeVertices`. Its
    // cost is the cheapest insertion detour over the route (for every route
    // edge u->v: time(u->p) + time(p->v) - time(u->v), by Dijkstra under
    // pure travel time), in tenths of a second, plus the fixed stop time for
    // a fuel station. Benefits are the documented Rules constants.
    //
    // Two rules keep the selection feasible in gameplay (the brief: "never
    // recommend a plan that violates ... capacity, pickup availability"):
    //  - at most (max charges - nitroCharges) nitro pickups are offered,
    //    the cheapest ones, since a charge beyond capacity is worthless;
    //  - fuel stations are offered only when `fuelNeeded` (the race's
    //    refuel plan is infeasible on the route as it stands).
    void BuildPickupItems(const model::Graph& graph, const DynArray<int>& routeVertices,
        const DynArray<int>& routeEdges, int nitroCharges, bool fuelNeeded,
        DynArray<algo::PickupItem>* outItems, DynArray<PickupInfo>* outInfo);

    enum class StationChoice
    {
        FirstOnRoute,   // Opponents 1 and 2: the first eligible station along their own route
        Nearest,         // Opponent 3 and the human autopilot: the nearest reachable station
    };

    // Plans a refuelling detour: current -> station (one Dijkstra search
    // under `weights`), then station -> every remaining checkpoint -> finish
    // (algo::PlanLap). Only stations reachable with `fuelTenths` are
    // eligible. FirstOnRoute picks the station adjacent to the EARLIEST
    // vertex of `referenceRoute` (falling back to Nearest if none is
    // adjacent); Nearest minimises the cost of the leg to the station.
    // Returns false if no active station is reachable.
    bool PlanStationDetour(const model::Graph& graph, int currentVertex, int fuelTenths,
        int lastCompletedCheckpoint, int finishVertex, const model::Weights& weights,
        const model::CostReferences& refs, StationChoice choice,
        const int* referenceRoute, int referenceLen,
        DynArray<int>* outVertices, DynArray<int>* outEdges, int* outStation);
}
