// smartrace_cli: headless driver and API usage sample.
//
// Phase 1 demo: loads each of the 3 real circuits, runs the Track Explorer
// queries, and plans the full lap (through every mandatory checkpoint, in
// order, to the finish) with both Dijkstra and A*, printing their costs
// side by side to show they agree.
#include "../src/api/SmartRaceApi.h"

#include <cstdio>

namespace
{
    const char* PresetName(Preset p)
    {
        switch (p)
        {
            case Preset::Speed: return "Speed";
            case Preset::Economy: return "Economy";
            case Preset::Safety: return "Safety";
            case Preset::Balanced:
            default: return "Balanced";
        }
    }

    void PrintRoute(const char* label, const RouteView& route)
    {
        if (!route.found)
        {
            printf("    %-10s NOT FOUND (blocked leg %d)\n", label, route.blockedLegIndex);
            return;
        }
        printf("    %-10s cost=%7.3f  dist=%7.1f  time=%6.2fs  fuel=%6.2f  (%d vertices)\n",
            label, route.totalCost, route.totalDistance, route.totalTime, route.totalFuel,
            route.vertexPath.count);
    }
}

int main()
{
    ApiStatus status = ApiInit("data");
    if (status != ApiStatus::Ok)
    {
        printf("ApiInit failed: %s\n", ApiLastError());
        return 1;
    }

    int circuitCount = ApiCircuitCount();
    printf("Circuits available: %d\n\n", circuitCount);

    for (int c = 0; c < circuitCount; ++c)
    {
        printf("================================================================\n");
        printf("[%d] %s\n", c, ApiCircuitName(c));

        status = ApiLoadCircuit(c);
        if (status != ApiStatus::Ok)
        {
            printf("  ApiLoadCircuit failed: %s\n", ApiLastError());
            continue;
        }

        TrackInfo info = ApiGetTrackInfo();
        printf("  vertices=%d edges=%d laps=%d checkpoints=%d shortcuts=%d cycle=%s deadEnd=%s validator=%s\n",
            info.vertexCount, info.edgeCount, info.laps, info.mandatoryCheckpointCount, info.shortcutCount,
            info.hasCycle ? "yes" : "no", info.hasDeadEnd ? "yes" : "no",
            info.validatorPassed ? "pass" : "FAIL");

        ComponentsView components = ApiRunDfsComponents();
        printf("  connected components: %d\n", components.componentCount);

        DeadEndsView deadEnds = ApiRunDeadEnds();
        printf("  dead-end vertices: %d\n", deadEnds.deadEndVertices.count);

        printf("\n  Full-lap route plans (Start -> every checkpoint in order -> Finish):\n");
        Preset presets[4] = { Preset::Balanced, Preset::Speed, Preset::Economy, Preset::Safety };
        for (int p = 0; p < 4; ++p)
        {
            RouteView dijkstra = ApiPlanLap(presets[p], RouteAlgo::Dijkstra);
            RouteView astar = ApiPlanLap(presets[p], RouteAlgo::AStar);

            printf("  Preset: %s\n", PresetName(presets[p]));
            PrintRoute("Dijkstra", dijkstra);
            PrintRoute("A*", astar);
        }

        RouteCompareView compare = ApiCompareRoutes(Preset::Balanced);
        printf("\n  Route comparison (Start -> Finish, %d candidates):\n", compare.candidates.count);
        for (int i = 0; i < compare.candidates.count; ++i)
        {
            const RouteCandidateView& cand = compare.candidates[i];
            printf("    %-14s cost=%7.3f dist=%7.1f time=%6.2fs fuel=%6.2f\n",
                cand.label, cand.totalCost, cand.totalDistance, cand.totalTime, cand.totalFuel);
        }

        printf("\n");
    }

    printf("================================================================\n");
    printf("Stub race (loaded circuit's MAINROUTE, 10 simulated seconds)...\n");
    ApiLoadCircuit(0);
    status = ApiRaceStart();
    if (status == ApiStatus::Ok)
    {
        for (int step = 0; step < 10; ++step)
        {
            ApiRaceStep(50);   // 50 * 20ms = 1 simulated second
            RaceSnapshot snap = ApiRaceSnapshot();
            printf("  t=%5dms ", snap.timeMs);
            for (int i = 0; i < snap.racers.count; ++i)
            {
                const RacerView& r = snap.racers[i];
                printf("| %-8s (%6.1f,%6.1f) ", r.name, r.x, r.y);
            }
            printf("\n");
        }
    }
    else
    {
        printf("  ApiRaceStart failed: %s\n", ApiLastError());
    }

    ApiShutdown();
    return 0;
}
