#pragma once

#include "../model/Graph.h"
#include "../model/Records.h"
#include "../model/CostModel.h"
#include "../io/TrackLoader.h"
#include "../core/ds/DynArray.h"

namespace sim
{
    struct StrategyPlan
    {
        StrategyPlan()
            : found(false)
        {
        }

        bool found;
        DynArray<int> vertexPath;
        DynArray<int> edgePath;
    };

    // Shared context every strategy needs. Owned by RaceEngine; borrowed
    // here for the duration of one PlanRoute call.
    struct StrategyContext
    {
        const DynArray<int>* mainRoute;              // the circuit's one-lap vertex sequence
        const DynArray<io::DetourRule*>* detours;       // Opponent 1's documented fixed detours
        const DynArray<int>* shortcutPreference;          // Opponent 2's preferred shortcut edge ids
        int finishVertex;
        model::CostReferences costReferences;
    };

    // Project.pdf section 4.4: opponents must behave identically given the
    // same track, event sequence and starting conditions -- every strategy
    // below is a pure function of (graph, racer, context), nothing random.
    //
    // Refuelling (the brief's "Refuelling" column): the race engine decides
    // WHEN a racer must refuel (below 25% of the tank, or not enough fuel
    // for the rest of its planned route) and routes it to a station via
    // sim/ResourcePlanner -- the first eligible station on its own route
    // for Opponents 1 and 2, the nearest reachable one for Opponent 3. A
    // strategy only says HOW MUCH to buy once stopped; the shared default
    // below buys the maximum allowed per stop for all three.
    class IStrategy
    {
    public:
        virtual ~IStrategy()
        {
        }

        // Produces the racer's route from its current vertex, through
        // every remaining mandatory checkpoint in order, to the finish.
        virtual StrategyPlan PlanRoute(const model::Graph& graph, const model::Racer& racer,
            const StrategyContext& context) = 0;

        virtual int RefuelAmount(const model::Racer& racer) const;
    };

    // Opponent 1: follows the circuit's documented MAINROUTE. If the next
    // main-route edge is blocked, substitutes the matching documented
    // DETOUR (if any); otherwise the route fails and the race engine's
    // recovery policy (wait, then DNF) takes over.
    class FixedRouteStrategy : public IStrategy
    {
    public:
        StrategyPlan PlanRoute(const model::Graph& graph, const model::Racer& racer,
            const StrategyContext& context) override;
    };

    // Opponent 2: follows MAINROUTE like Opponent 1, but substitutes any
    // of its preferred shortcuts whenever the shortcut's starting vertex
    // lies on the remaining route and the shortcut is currently open —
    // splicing past the main-route vertices the shortcut bypasses. Falls
    // back toward the main route (and Opponent 1's detour handling) if a
    // shortcut it was using closes.
    class ShortcutLoverStrategy : public IStrategy
    {
    public:
        StrategyPlan PlanRoute(const model::Graph& graph, const model::Racer& racer,
            const StrategyContext& context) override;
    };

    // Opponent 3: always takes the available route with minimum total
    // distance (Dijkstra under the Distance preset), recomputed fresh
    // every time it is asked to plan.
    class ShortestPathStrategy : public IStrategy
    {
    public:
        StrategyPlan PlanRoute(const model::Graph& graph, const model::Racer& racer,
            const StrategyContext& context) override;
    };
}
