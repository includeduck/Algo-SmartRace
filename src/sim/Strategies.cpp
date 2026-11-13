#include "Strategies.h"
#include "../algo/RoutePlanner.h"
#include "../model/Rules.h"

namespace sim
{
    int IStrategy::RefuelAmount(const model::Racer& racer) const
    {
        double threshold = Rules::kMaxFuelTenths * Rules::kOpponentLowFuelThreshold;
        if (racer.fuelTenths < threshold) return Rules::kMaxStationPurchaseTenths;
        return -1;
    }

    namespace
    {
        // Walks `mainRoute` forward from `fromVertex`'s first occurrence
        // until `toVertex` is reached (wrapping around once, since the
        // route is one closed lap), appending every vertex visited
        // (inclusive of both ends) to `outVertices`. Returns false if
        // either vertex is not found within one full loop.
        //
        // When fromVertex == toVertex (both are the finish vertex: the
        // racer is standing at the finish, having just reset to start a
        // new lap), this deliberately does NOT short-circuit to a trivial
        // one-vertex "already there" result — the step loop below only
        // matches from step=1 onward, so it walks one full lap around
        // back to the same vertex, which is exactly the route the racer
        // needs to drive next. (The only other way fromVertex could equal
        // toVertex is the racer's final completed lap, but ArriveAtVertex
        // transitions to Finished before ever asking for another plan, so
        // that case never reaches here.)
        bool BuildMainRouteLeg(const DynArray<int>& mainRoute, int fromVertex, int toVertex,
            DynArray<int>* outVertices)
        {
            int n = mainRoute.Size();
            if (n == 0) return false;

            int fromIdx = -1;
            for (int i = 0; i < n; ++i)
            {
                if (mainRoute[i] == fromVertex)
                {
                    fromIdx = i;
                    break;
                }
            }
            if (fromIdx == -1) return false;

            outVertices->Push(fromVertex);
            for (int step = 1; step <= n; ++step)
            {
                int idx = (fromIdx + step) % n;
                outVertices->Push(mainRoute[idx]);
                if (mainRoute[idx] == toVertex) return true;
            }
            return false;
        }

        // Converts a vertex-sequence leg into a StrategyPlan, substituting
        // Opponent 1's documented detour for the FIRST blocked edge found
        // (if a matching one exists). Any edge still blocked after that
        // makes the whole plan fail (the race engine's recovery policy
        // then applies), matching "otherwise wait/recover".
        bool ResolveWithDetours(const model::Graph& graph, DynArray<int>& vertices,
            const DynArray<io::DetourRule*>* detours, StrategyPlan* outPlan)
        {
            DynArray<int> edges;
            bool ok = algo::BuildEdgePathFromVertices(graph, vertices, &edges);

            if (!ok && detours != nullptr)
            {
                // Find the first consecutive pair with no traversable edge
                // and see if a documented detour covers it.
                for (int i = 0; i + 1 < vertices.Size(); ++i)
                {
                    int from = vertices[i];
                    int to = vertices[i + 1];
                    int directEdge = -1;
                    for (const model::AdjNode* node = graph.Neighbors(from); node != nullptr; node = node->next)
                    {
                        if (node->neighbor == to && graph.IsTraversable(node->edgeId))
                        {
                            directEdge = node->edgeId;
                            break;
                        }
                    }
                    if (directEdge != -1) continue;   // this pair is fine, keep looking

                    // Find the blocked edge id between from/to (even though
                    // not traversable) to match against a detour rule.
                    int blockedEdge = -1;
                    for (const model::AdjNode* node = graph.Neighbors(from); node != nullptr; node = node->next)
                    {
                        if (node->neighbor == to)
                        {
                            blockedEdge = node->edgeId;
                            break;
                        }
                    }

                    const io::DetourRule* matching = nullptr;
                    for (int d = 0; d < detours->Size() && matching == nullptr; ++d)
                    {
                        if ((*detours)[d]->blockedEdgeId == blockedEdge) matching = (*detours)[d];
                    }
                    if (matching == nullptr) return false;

                    // matching->viaVertices is a COMPLETE path [from, ..., to]
                    // for the blocked edge (by the .trk convention: it starts
                    // and ends at the same two vertices the blocked edge
                    // connects). vertices[i] (== from) and the loop below
                    // starting at vertices[i+1] (== to) already supply those
                    // endpoints, so only the MIDDLE of viaVertices is spliced
                    // in -- including its first/last entries again would
                    // insert a duplicate consecutive vertex (a bogus
                    // self-loop with no edge), silently making the whole
                    // detour unresolvable.
                    DynArray<int> spliced;
                    for (int k = 0; k <= i; ++k) spliced.Push(vertices[k]);
                    for (int k = 1; k + 1 < matching->viaVertices.Size(); ++k) spliced.Push(matching->viaVertices[k]);
                    for (int k = i + 1; k < vertices.Size(); ++k) spliced.Push(vertices[k]);

                    vertices.Clear();
                    for (int k = 0; k < spliced.Size(); ++k) vertices.Push(spliced[k]);

                    edges.Clear();
                    ok = algo::BuildEdgePathFromVertices(graph, vertices, &edges);
                    break;
                }
            }

            if (!ok) return false;

            outPlan->found = true;
            for (int i = 0; i < vertices.Size(); ++i) outPlan->vertexPath.Push(vertices[i]);
            for (int i = 0; i < edges.Size(); ++i) outPlan->edgePath.Push(edges[i]);
            return true;
        }
    }

    StrategyPlan FixedRouteStrategy::PlanRoute(const model::Graph& graph, const model::Racer& racer,
        const StrategyContext& context)
    {
        StrategyPlan plan;
        if (context.mainRoute == nullptr) return plan;

        DynArray<int> vertices;
        if (!BuildMainRouteLeg(*context.mainRoute, racer.currentVertex, context.finishVertex, &vertices))
        {
            return plan;
        }

        ResolveWithDetours(graph, vertices, context.detours, &plan);
        return plan;
    }

    StrategyPlan ShortcutLoverStrategy::PlanRoute(const model::Graph& graph, const model::Racer& racer,
        const StrategyContext& context)
    {
        StrategyPlan plan;
        if (context.mainRoute == nullptr) return plan;

        DynArray<int> vertices;
        if (!BuildMainRouteLeg(*context.mainRoute, racer.currentVertex, context.finishVertex, &vertices))
        {
            return plan;
        }

        // Splice in any open preferred shortcut whose starting vertex lies
        // on the route, jumping forward to the shortcut's far endpoint
        // (wherever it next appears) and skipping whatever main-route
        // vertices lay between them.
        if (context.shortcutPreference != nullptr)
        {
            for (int i = 0; i < vertices.Size(); ++i)
            {
                int from = vertices[i];
                for (int s = 0; s < context.shortcutPreference->Size(); ++s)
                {
                    int shortcutEdge = (*context.shortcutPreference)[s];
                    const model::Edge& e = graph.GetEdge(shortcutEdge);
                    if (e.from != from || !graph.IsTraversable(shortcutEdge)) continue;

                    int toVertex = e.to;
                    int laterIdx = -1;
                    for (int k = i + 1; k < vertices.Size(); ++k)
                    {
                        if (vertices[k] == toVertex)
                        {
                            laterIdx = k;
                            break;
                        }
                    }
                    if (laterIdx <= i + 1) continue;   // not a real shortcut forward, skip

                    DynArray<int> spliced;
                    for (int k = 0; k <= i; ++k) spliced.Push(vertices[k]);
                    spliced.Push(toVertex);
                    for (int k = laterIdx + 1; k < vertices.Size(); ++k) spliced.Push(vertices[k]);

                    vertices.Clear();
                    for (int k = 0; k < spliced.Size(); ++k) vertices.Push(spliced[k]);
                    break;
                }
            }
        }

        ResolveWithDetours(graph, vertices, context.detours, &plan);
        return plan;
    }

    StrategyPlan ShortestPathStrategy::PlanRoute(const model::Graph& graph, const model::Racer& racer,
        const StrategyContext& context)
    {
        StrategyPlan plan;
        model::Weights distanceWeights = model::WeightsForPreset(model::Preset::Distance);

        algo::PlanResult result = algo::PlanLap(graph, racer.currentVertex, racer.nextCheckpoint - 1,
            context.finishVertex, distanceWeights, context.costReferences, algo::RouteAlgo::Dijkstra);

        plan.found = result.found;
        if (!result.found) return plan;

        for (int i = 0; i < result.vertexPath.Size(); ++i) plan.vertexPath.Push(result.vertexPath[i]);
        for (int i = 0; i < result.edgePath.Size(); ++i) plan.edgePath.Push(result.edgePath[i]);
        return plan;
    }
}
