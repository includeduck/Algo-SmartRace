// smartrace_cli: headless driver and API usage sample.
//
// Phase 3 demo:
//   1. per circuit: track info, Dijkstra vs A* lap costs, route comparison,
//      the refuelling DP's plan and the pickup knapsack (DP vs greedy);
//   2. a full championship (C1 -> C2 -> C3), printing each race's event
//      log and results, then the standings by Merge, Quick and Heap Sort
//      and the top 3 by the size-k heap.
//   3. Phase 4: every Algorithm Lab module's trace on the loaded circuit
//      (sorts rank the final standings), then the backtracking generator's
//      attempts, its track saved, reloaded and run through the validator.
#include "../src/api/SmartRaceApi.h"

#include <cstdio>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace
{
    // Debug builds: the MSVC CRT debug heap reports every block still
    // allocated when the process exits (after static destructors), on
    // stderr, as "Detected memory leaks!". Release builds: no-op.
    void EnableLeakReport()
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif
    }
}

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

    const char* SortName(SortMethod m)
    {
        switch (m)
        {
            case SortMethod::Quick: return "Quick";
            case SortMethod::Heap: return "Heap";
            case SortMethod::Merge:
            default: return "Merge";
        }
    }

    int LapOrDash(int ms)
    {
        return (ms >= 0x7fffffff) ? -1 : ms;
    }

    void PrintExplorer(int circuit)
    {
        printf("================================================================\n");
        printf("[%d] %s\n", circuit, ApiCircuitName(circuit));

        if (ApiLoadCircuit(circuit) != ApiStatus::Ok)
        {
            printf("  ApiLoadCircuit failed: %s\n", ApiLastError());
            return;
        }

        TrackInfo info = ApiGetTrackInfo();
        printf("  vertices=%d edges=%d checkpoints=%d shortcuts=%d cycle=%s deadEnd=%s validator=%s\n",
            info.vertexCount, info.edgeCount, info.mandatoryCheckpointCount, info.shortcutCount,
            info.hasCycle ? "yes" : "no", info.hasDeadEnd ? "yes" : "no", info.validatorPassed ? "pass" : "FAIL");

        printf("  Full lap, Dijkstra vs A* (cost must match):\n");
        Preset presets[4] = { Preset::Balanced, Preset::Speed, Preset::Economy, Preset::Safety };
        for (int p = 0; p < 4; ++p)
        {
            RouteView d = ApiPlanLap(presets[p], RouteAlgo::Dijkstra);
            RouteView a = ApiPlanLap(presets[p], RouteAlgo::AStar);
            printf("    %-9s dijkstra=%7.3f  a*=%7.3f  dist=%6.0f time=%5.1fs fuel=%5.1f\n",
                PresetName(presets[p]), d.totalCost, a.totalCost, d.totalDistance, d.totalTime, d.totalFuel);
        }

        RouteCompareView compare = ApiCompareRoutes(Preset::Balanced);
        printf("  Start -> Finish candidates (scored as Balanced):\n");
        for (int i = 0; i < compare.candidates.count; ++i)
        {
            const RouteCandidateView& c = compare.candidates[i];
            printf("    %-14s cost=%6.3f dist=%6.0f time=%5.1fs\n", c.label, c.totalCost, c.totalDistance, c.totalTime);
        }

        RefuelView refuel = ApiPlanRefuel(Preset::Balanced);
        printf("  Refuelling DP over the whole race (Balanced route): %s",
            refuel.feasible ? "feasible" : "INFEASIBLE without a station detour");
        if (refuel.feasible) printf(", %d stop(s), %.1fs stopped", refuel.stopCount, refuel.totalStopTimeMs / 1000.0);
        printf("\n");

        PickupView pickups = ApiPlanPickups(Preset::Balanced, 150);
        printf("  Pickups within a 15.0s detour budget (fuel needed: %s, DP filled %lld cells):\n",
            pickups.fuelNeeded ? "yes" : "no", pickups.dpStates);
        for (int i = 0; i < pickups.items.count; ++i)
        {
            const PickupItemView& item = pickups.items[i];
            printf("    V%-3d %-6s cost=%4d benefit=%4d  dp=%s greedy=%s\n", item.vertex,
                item.isNitro ? "nitro" : "fuel", item.costTenthsSec, item.benefitTenthsSec,
                item.chosenByDp ? "take" : "-", item.chosenByGreedy ? "take" : "-");
        }
        printf("    DP benefit=%d (cost %d)   greedy benefit=%d (cost %d)\n\n",
            pickups.dpBenefit, pickups.dpCost, pickups.greedyBenefit, pickups.greedyCost);
    }

    void RunRaceToCompletion()
    {
        int lastLogIndex = -1;
        int ticks = 0;
        const int kMaxTicks = 20 * 60 * 60 * 50;
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
        printf("  Race over at t=%dms (%d ticks)\n", ApiRaceSnapshot().timeMs, ticks);
    }

    void PrintStandings()
    {
        SortMethod methods[3] = { SortMethod::Merge, SortMethod::Quick, SortMethod::Heap };
        for (int m = 0; m < 3; ++m)
        {
            StandingsView s = ApiStandings(methods[m]);
            printf("  %-5s sort: %3lld comparisons, %3lld moves/swaps |", SortName(methods[m]), s.comparisons, s.swaps);
            for (int i = 0; i < s.rows.count; ++i) printf(" %s", s.rows[i].name);
            printf("\n");
        }

        StandingsView s = ApiStandings(SortMethod::Merge);
        printf("  All three sorts agree: %s\n", s.allMethodsAgree ? "yes" : "NO");
        for (int i = 0; i < s.rows.count; ++i)
        {
            const StandingRowView& r = s.rows[i];
            printf("    %d. %-8s %3d pts  wins=%d  sumPos=%2d  bestLap=%6dms\n",
                r.position, r.name, r.totalPoints, r.wins, r.sumOfFinishPositions, LapOrDash(r.bestFastestLapMs));
        }

        TopKView top = ApiTopK(3);
        printf("  Top 3 via size-k heap (%lld comparisons vs %lld for a full Merge Sort, matches prefix: %s):",
            top.comparisons, top.fullSortComparisons, top.matchesSortPrefix ? "yes" : "NO");
        for (int i = 0; i < top.rows.count; ++i) printf(" %s", top.rows[i].name);
        printf("\n");
    }
}

namespace
{
    const char* const kLabNames[13] =
    {
        "BFS", "DFS", "Dijkstra", "A*", "Kruskal", "TrackGenerator", "KnapsackDp", "GreedyPickup",
        "RefuelDp", "MergeSort", "QuickSort", "HeapSort", "EventQueue",
    };

    void PrintLab()
    {
        printf("================================================================\n");
        printf("ALGORITHM LAB on %s (sorts rank the final standings)\n", ApiCircuitName(0));
        if (ApiLoadCircuit(0) != ApiStatus::Ok)
        {
            printf("ApiLoadCircuit failed: %s\n", ApiLastError());
            return;
        }

        LabInput input = ApiLabDefaultInput();
        for (int i = 0; i <= static_cast<int>(LabAlgo::EventQueue); ++i)
        {
            LabAlgo algo = static_cast<LabAlgo>(i);
            if (ApiLabRun(algo, input) != ApiStatus::Ok)
            {
                printf("  %-14s failed: %s\n", kLabNames[i], ApiLastError());
                continue;
            }
            int steps = ApiLabStepCount();
            LabFrame last = ApiLabFrame(steps - 1);
            PseudocodeView code = ApiLabPseudocode(algo);
            const char* lastLine = (last.line >= 0 && last.line < code.lines.count) ? code.lines[last.line] : "?";
            printf("  %-14s %6d steps  last line %2d: %s\n", kLabNames[i], steps, last.line, lastLine);
            printf("  %-14s %s\n", "", ApiLabResultText());
        }
    }

    void PrintGenerator()
    {
        printf("================================================================\n");
        printf("TRACK GENERATOR (seed 1) -> build/generated_demo.trk -> validator\n");

        LabInput input = ApiLabDefaultInput();
        if (ApiLabRun(LabAlgo::TrackGenerator, input) != ApiStatus::Ok)
        {
            printf("ApiLabRun failed: %s\n", ApiLastError());
            return;
        }

        GeneratorReportView report = ApiLabGeneratorReport();
        for (int i = 0; i < report.attempts.count; ++i)
        {
            const GeneratorAttemptView& a = report.attempts[i];
            printf("  attempt %d: %s  (%lld nodes explored, %lld branches pruned)  %s\n", a.attempt,
                a.accepted ? "ACCEPTED" : "rejected", a.nodesExplored, a.prunedBranches, a.reason);
        }
        if (report.acceptedAttempt < 0) return;

        if (ApiLabSaveGenerated("build/generated_demo.trk") != ApiStatus::Ok
            || ApiLoadCircuitFile("build/generated_demo.trk") != ApiStatus::Ok)
        {
            printf("save/reload failed: %s\n", ApiLastError());
            return;
        }

        TrackInfo info = ApiGetTrackInfo();
        printf("  reloaded \"%s\": %d laps\n", info.name, info.laps);
        ValidationView v = ApiValidateTrack();
        for (int i = 0; i < v.checks.count; ++i)
        {
            printf("    [%s] %-28s %s\n", v.checks[i].passed ? "ok" : "XX", v.checks[i].name, v.checks[i].detail);
        }
        printf("  validator: %s\n", v.passed ? "PASSED" : "FAILED");
    }
}

int main()
{
    EnableLeakReport();
    if (ApiInit("data") != ApiStatus::Ok)
    {
        printf("ApiInit failed: %s\n", ApiLastError());
        return 1;
    }

    for (int c = 0; c < ApiCircuitCount(); ++c) PrintExplorer(c);

    printf("================================================================\n");
    printf("CHAMPIONSHIP: 1 human (autopilot) + 3 opponents, circuits 0 -> 1 -> 2\n");

    if (ApiChampionshipStart() != ApiStatus::Ok)
    {
        printf("ApiChampionshipStart failed: %s\n", ApiLastError());
        return 1;
    }

    for (;;)
    {
        ChampionshipStatus status = ApiChampionshipGetStatus();
        if (status.complete) break;

        printf("\n--- Race %d of %d: %s ---\n", status.racesCompleted + 1, status.racesTotal,
            ApiCircuitName(status.currentCircuitIndex));

        if (ApiRaceStart() != ApiStatus::Ok)
        {
            printf("ApiRaceStart failed: %s\n", ApiLastError());
            break;
        }
        RunRaceToCompletion();

        ResultsView results = ApiRaceResults();
        printf("  Results:\n");
        for (int i = 0; i < results.rows.count; ++i)
        {
            const ResultRowView& r = results.rows[i];
            printf("    P%d racer %d  %-9s time=%6dms bestLap=%6dms points=%2d%s\n", r.finishPosition, r.racerId,
                StatusName(r.status), r.finishTimeMs, LapOrDash(r.bestLapTimeMs), r.pointsAwarded,
                r.bonusPoints > 0 ? " (incl. fastest-lap bonus)" : "");
        }

        if (ApiChampionshipNextRace() != ApiStatus::Ok)
        {
            printf("ApiChampionshipNextRace failed: %s\n", ApiLastError());
            break;
        }

        printf("  Standings after race %d:\n", ApiChampionshipGetStatus().racesCompleted);
        PrintStandings();
    }

    ChampionshipStatus final = ApiChampionshipGetStatus();
    if (final.complete)
    {
        StandingsView s = ApiStandings(SortMethod::Merge);
        printf("\nCHAMPION: %s (racer %d)\n", s.rows.count > 0 ? s.rows[0].name : "?", final.championId);
    }

    PrintLab();
    PrintGenerator();

    ApiShutdown();
    return 0;
}
