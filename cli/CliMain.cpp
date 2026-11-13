// smartrace_cli: headless driver and API usage sample.
//
// Phase 2 demo: after the Phase 1 route-planning printout, runs a full
// deterministic race on the first circuit (1 human on autopilot + 3
// opponents) to completion, printing the event log and final standings.
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

    const char* StatusName(int status)
    {
        switch (status)
        {
            case 0: return "Finished";
            case 1: return "DNF";
            case 2: return "TimeLimit";
            default: return "?";
        }
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

    for (int c = 0; c < circuitCount; ++c)
    {
        printf("================================================================\n");
        printf("Full race on circuit %d, %s (1 human autopilot + 3 opponents)...\n\n", c, ApiCircuitName(c));

        ApiLoadCircuit(c);
        status = ApiRaceStart();
        if (status != ApiStatus::Ok)
        {
            printf("ApiRaceStart failed: %s\n", ApiLastError());
            continue;
        }

        int lastLogIndex = -1;
        int ticks = 0;
        const int kMaxTicks = 20 * 60 * 60 * 50;   // generous cap
        while (!ApiRaceOver() && ticks < kMaxTicks)
        {
            ApiRaceStep(1);
            ++ticks;

            LogView log = ApiRaceLogSince(lastLogIndex);
            for (int i = 0; i < log.lines.count; ++i)
            {
                printf("  [t=%6dms] %s\n", log.lines[i].timeMs, log.lines[i].text);
                lastLogIndex = log.lines[i].index;
            }
        }

        printf("\nRace over at t=%dms after %d ticks.\n\n", ApiRaceSnapshot().timeMs, ticks);

        printf("Final standings:\n");
        ResultsView results = ApiRaceResults();
        for (int i = 0; i < results.rows.count; ++i)
        {
            const ResultRowView& r = results.rows[i];
            printf("  P%-2d racer=%-2d status=%-10s finishTime=%6dms bestLap=%6dms points=%2d (+%d bonus)\n",
                r.finishPosition, r.racerId, StatusName(r.status), r.finishTimeMs,
                (r.bestLapTimeMs >= 0x7fffffff) ? -1 : r.bestLapTimeMs, r.pointsAwarded, r.bonusPoints);
        }
        printf("\n");
    }

    ApiShutdown();
    return 0;
}
