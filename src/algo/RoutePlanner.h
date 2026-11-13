#pragma once

#include "PathResult.h"
#include "../model/CostModel.h"
#include "../core/util/String.h"
#include "AlgoStats.h"
#include "Trace.h"

namespace algo
{
    enum class RouteAlgo
    {
        Dijkstra,
        AStar,
    };

    struct LegResult
    {
        LegResult()
            : fromVertex(-1), toVertex(-1), found(false)
        {
        }

        int fromVertex;
        int toVertex;    // the checkpoint/finish vertex this leg targets
        PathResult path;
        bool found;
    };

    // Project.pdf section 5.2's "checkpoint-aware routing rule": a route
    // must never skip a mandatory checkpoint. PlanLap enforces this by
    // construction, not by checking afterward — it plans one leg at a time,
    // each a full Dijkstra/A* search from the previous leg's end to the
    // NEXT required checkpoint (in checkpointOrder order), then finally to
    // the finish. If any leg is unreachable, planning stops there;
    // blockedLegIndex says which leg failed, and the caller applies the
    // race's recovery policy (Phase 2).
    struct PlanResult
    {
        PlanResult()
            : found(false), blockedLegIndex(-1),
              totalCost(0.0), totalDistance(0.0), totalTime(0.0), totalFuel(0.0)
        {
        }

        ~PlanResult();
        PlanResult(const PlanResult&) = delete;
        PlanResult& operator=(const PlanResult&) = delete;

        // Hand-written, not `= default` -- see AlternativeRoutesResult's
        // comment in Dfs.h for why: this struct owns `legs`.
        PlanResult(PlanResult&& other) noexcept;
        PlanResult& operator=(PlanResult&& other) noexcept;

        bool found;             // true iff every leg succeeded
        int blockedLegIndex;     // -1 if found, else the first leg that failed

        DynArray<LegResult*> legs;   // owned, one per leg, in order
        DynArray<int> vertexPath;      // concatenated full route (only meaningful if found)
        DynArray<int> edgePath;

        double totalCost, totalDistance, totalTime, totalFuel;
    };

    // `lastCompletedCheckpoint` is the highest checkpointOrder the racer
    // has already visited in order (0 if none yet this lap). Re-plan after
    // every graph-changing event or lap-boundary crossing.
    PlanResult PlanLap(const model::Graph& graph, int currentVertex, int lastCompletedCheckpoint,
        int finishVertex, const model::Weights& weights, const model::CostReferences& refs,
        RouteAlgo algo, AlgoStats* stats = nullptr, Trace* trace = nullptr);

    struct RouteCandidate
    {
        RouteCandidate()
            : found(false), totalCost(0.0), totalDistance(0.0), totalTime(0.0), totalFuel(0.0)
        {
        }

        String label;    // e.g. "Balanced", "Speed", "Alternative 1"
        bool found;
        DynArray<int> vertexPath;
        DynArray<int> edgePath;
        double totalCost, totalDistance, totalTime, totalFuel;   // all under the comparison's evalWeights
    };

    struct RouteCompareResult
    {
        RouteCompareResult()
        {
        }
        ~RouteCompareResult();
        RouteCompareResult(const RouteCompareResult&) = delete;
        RouteCompareResult& operator=(const RouteCompareResult&) = delete;

        // Hand-written, not `= default` -- see AlternativeRoutesResult's
        // comment in Dfs.h for why: this struct owns `candidates`.
        RouteCompareResult(RouteCompareResult&& other) noexcept;
        RouteCompareResult& operator=(RouteCompareResult&& other) noexcept;

        DynArray<RouteCandidate*> candidates;   // owned, up to Rules::kAlternativeRouteCap-bounded count
    };

    // Up to 4 candidate routes between `source` and `target`: one Dijkstra
    // route per preset (Balanced/Speed/Economy/Safety), deduplicated by
    // identical edge sequence, plus bounded DFS alternatives relative to
    // the first candidate found. Every candidate's displayed totals are
    // computed under `evalWeights`, so the comparison table is apples to
    // apples even though each preset route was found by optimizing its own
    // weights (Project.pdf section 5.2: "compare at least two candidate
    // routes").
    RouteCompareResult CompareRoutes(const model::Graph& graph, int source, int target,
        const model::Weights& evalWeights, const model::CostReferences& refs,
        AlgoStats* stats = nullptr, Trace* trace = nullptr);

    // Resolves the edge actually used between each consecutive pair of a
    // vertex sequence (cheapest-by-distance if parallel edges connect the
    // same pair), appending to `outEdges`. Returns false if some
    // consecutive pair has no traversable connecting edge at all. Shared by
    // CompareRoutes' alternative-route candidates and by the opponent
    // strategies (sim/Strategies), which build vertex sequences of their
    // own (main-route following, shortcut/detour splicing) and need the
    // same vertices-to-edges resolution.
    bool BuildEdgePathFromVertices(const model::Graph& graph, const DynArray<int>& vertexPath,
        DynArray<int>* outEdges);
}
