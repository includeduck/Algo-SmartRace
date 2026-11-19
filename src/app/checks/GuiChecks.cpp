#include "api/SmartRaceApi.h"
#include "ui/Format.h"
#include "ui/OwnedArray.h"
#include "ui/RaceClock.h"
#include "ui/LabSource.h"
#include "ui/LabEventStatus.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <climits>

namespace
{
    int g_failures = 0;
    int g_checks = 0;

    void Check(bool pass, const char* description)
    {
        ++g_checks;
        if (!pass)
        {
            ++g_failures;
            printf("FAIL: %s\n", description);
        }
    }

    void FormatAndClock()
    {
        char text[32];
        Format::Time(text, sizeof(text), -1);
        Check(strcmp(text, "--:--.---") == 0, "DNF sentinel is not displayed as a time");
        Format::Time(text, sizeof(text), INT_MAX);
        Check(strcmp(text, "--:--.---") == 0, "missing-lap sentinel is not a huge time");
        Format::Time(text, sizeof(text), 83456);
        Check(strcmp(text, "1:23.456") == 0, "race-time units are milliseconds");
        RaceClock clock;
        int ticks = 0;
        for (int i = 0; i < 100; ++i) ticks += clock.Accumulate(0.01f, 1);
        Check(ticks == 50, "100 short frames produce 50 deterministic ticks");
        clock.Reset();
        ticks = 0;
        for (int i = 0; i < 10; ++i) ticks += clock.Accumulate(0.1f, 1);
        Check(ticks == 50, "different frame grouping preserves tick count");
        clock.Reset();
        Check(clock.Accumulate(10, 1) == 12, "window stalls have bounded catch-up");
        clock.Reset();
        Check(clock.Accumulate(0.01f, 1) == 0, "resume discards stale accumulated time");
    }

    void ExplorerContracts()
    {
        Check(ApiInit("build/missing-gui-data") == ApiStatus::Ok, "startup alone does not validate the data path");
        Check(ApiLoadCircuit(0) == ApiStatus::IoError, "missing data produces a recoverable load failure");
        Check(ApiGetGraph().vertices.count == 0, "failed load does not expose a stale graph");
        Check(ApiInit("data") == ApiStatus::Ok, "GUI initializes the existing API");
        Check(ApiCircuitCount() == 3, "three authored circuits are listed");
        for (int circuit = 0; circuit < ApiCircuitCount(); ++circuit)
        {
            if (ApiLoadCircuit(circuit) != ApiStatus::Ok)
            {
                Check(false, ApiLastError());
                continue;
            }
            Check(ApiValidateTrack().passed, "authored circuit validator passes");
            TrackInfo info = ApiGetTrackInfo();
            Check(info.mandatoryCheckpointCount >= 10 && info.shortcutCount >= 2, "minimum circuit scope is exposed");
            ReachabilityView same = ApiRunBfs(0, 0);
            Check(same.found && same.path.count == 1, "source equals target is a zero-road BFS path");
            for (int preset = 0; preset < 4; ++preset)
            {
                RouteView d = ApiPlanLap(static_cast<Preset>(preset), RouteAlgo::Dijkstra);
                bool found = d.found;
                double cost = d.totalCost;
                OwnedArray<int> edges;
                edges.Copy(d.edgePath);
                int count = edges.View().count;
                RouteView a = ApiPlanLap(static_cast<Preset>(preset), RouteAlgo::AStar);
                Check(found && a.found && fabs(cost - a.totalCost) < 0.000001, "Dijkstra/A* agree on the real circuit under the same preset");
                Check(edges.View().count == count && count > 0, "GUI path copy survives subsequent planner calls");
            }
        }
    }

    void RaceAndChampionship()
    {
        Settings settings = ApiGetSettings();
        settings.laps = 2;
        settings.opponentCount = 3;
        Check(ApiSetSettings(settings) == ApiStatus::Ok, "valid GUI settings accepted");
        Check(ApiChampionshipStart() == ApiStatus::Ok, "championship starts with circuit zero");
        Check(ApiChampionshipNextRace() != ApiStatus::Ok, "unfinished race cannot be recorded");
        for (int race = 0; race < 3; ++race)
        {
            Check(ApiRaceStart() == ApiStatus::Ok, "race starts once");
            int before = ApiRaceSnapshot().timeMs;
            ApiRaceStep(1);
            Check(ApiRaceSnapshot().timeMs == before + 20, "one GUI simulation tick is 20 ms");
            int frozen = ApiRaceSnapshot().timeMs;
            LabInput input = ApiLabDefaultInput();
            Check(ApiLabRun(LabAlgo::Bfs, input) == ApiStatus::Ok, "Lab can run on the paused current track");
            ApiLabFrame(ApiLabStepCount() - 1);
            ApiLabFrame(-1);
            Check(ApiRaceSnapshot().timeMs == frozen, "Lab replay does not advance race time");
            for (int tick = 0; tick < 50000 && !ApiRaceOver(); ++tick) ApiRaceStep(1);
            Check(ApiRaceOver(), "race completes or classifies its racers");
            Check(ApiRaceResults().rows.count == 4, "results contain human plus three opponents");
            Check(ApiChampionshipNextRace() == ApiStatus::Ok, "record finished race and load next circuit");
            Check(ApiChampionshipGetStatus().racesCompleted == race + 1, "each Continue records exactly one race");
            Check(ApiChampionshipNextRace() != ApiStatus::Ok, "duplicate Continue is rejected");
        }
        Check(ApiChampionshipGetStatus().complete, "champion is declared after circuit three");
        int ids[10] = {};
        StandingsView baseline = ApiStandings(SortMethod::Merge);
        int count = baseline.rows.count;
        for (int i = 0; i < count && i < 10; ++i) ids[i] = baseline.rows[i].racerId;
        for (int method = 0; method < 3; ++method)
        {
            StandingsView view = ApiStandings(static_cast<SortMethod>(method));
            Check(view.allMethodsAgree && view.rows.count == count, "dashboard sorting methods agree");
            for (int i = 0; i < count; ++i) Check(view.rows[i].racerId == ids[i], "ranked identity matches common comparator");
        }
        for (int k = 1; k <= count; ++k)
        {
            TopKView top = ApiTopK(k);
            Check(top.matchesSortPrefix && top.rows.count == k, "top-k matches full-sort prefix");
        }
    }

    void V11PresentationContracts()
    {
        Check(SMARTRACE_API_VERSION >= 11, "GUI requires the additive v1.1 contract");
        Check(strcmp(LabSource::Name(LabDataSource::BriefExampleFallback), "EXAMPLE FALLBACK") == 0, "example fallback has an explicit badge");
        Check(ApiLoadCircuit(0) == ApiStatus::Ok, "load Harbor for presentation contracts");
        RouteView zero = ApiPlanRoute(0, 0, Preset::Balanced, RouteAlgo::AStar);
        Check(zero.found && zero.totalCost == 0 && zero.edgePath.count == 0 && zero.vertexPath.count == 1, "endpoint summary handles source equals target");
        RouteView invalid = ApiPlanRoute(-2, 0, Preset::Balanced, RouteAlgo::Dijkstra);
        Check(!invalid.found && ApiLastError()[0] != '\0', "invalid endpoints provide an actionable message");
        RouteCompareView race = ApiCompareRaceRoutes(Preset::Balanced);
        Check(race.candidates.count > 0, "ordered candidates are available before a race");
        for (int i = 0; i < race.candidates.count; ++i)
        {
            const RouteCandidateView& candidate = race.candidates[i];
            Check(candidate.sourcePreset >= 0 && candidate.sourcePreset < 4, "only real preset candidates can be applied");
            Check(candidate.edgePath.count == candidate.vertexPath.count - 1, "candidate overlay has exact road identities");
            OwnedArray<int> overlay;
            overlay.Copy(candidate.edgePath);
            ArrayView<double> costs = ApiEdgeCosts(Preset::Balanced);
            double sum = 0;
            for (int j = 0; j < overlay.View().count; ++j)
            {
                int edge = overlay.View()[j];
                Check(edge >= 0 && edge < costs.count && costs[edge] >= 0, "overlay roads have valid nonnegative scalar labels");
                if (edge >= 0 && edge < costs.count) sum += costs[edge];
            }
            Check(fabs(sum - candidate.totalCost) < 0.000001, "displayed road costs sum to candidate scalar cost");
        }
        LabInput input = ApiLabDefaultInput();
        input.useBriefExample = true;
        Check(ApiLabRun(LabAlgo::KnapsackDp, input) == ApiStatus::Ok, "brief Lab run succeeds");
        LabRunInfo info = ApiLabRunInfo();
        Check(info.source == LabDataSource::BriefExample, "badge uses structured metadata rather than result prose");
        Check(ApiRaceStart() == ApiStatus::Ok, "start live presentation session");
        Check(ApiCmdAutopilot(false) == ApiStatus::Ok, "pause human decisions while events advance");
        while (!ApiRaceOver() && ApiRaceSnapshot().timeMs < 40100) ApiRaceStep(1);
        int frozen = ApiRaceSnapshot().timeMs;
        input = ApiLabDefaultInput();
        input.source = 0;
        input.target = 1;
        input.useRaceState = true;
        Check(ApiLabRun(LabAlgo::Dijkstra, input) == ApiStatus::Ok, "weighted Lab runs on the live changed graph");
        info = ApiLabRunInfo();
        Check(info.source == LabDataSource::LiveRace && info.sourceVertex == 0 && info.targetVertex == 1, "live badge and selected endpoint markers use actual metadata");
        ApiLabFrame(ApiLabStepCount() - 1);
        ApiLabFrame(-1);
        Check(ApiRaceSnapshot().timeMs == frozen, "seeking a live trace preserves race clock");
        Check(ApiLabRun(LabAlgo::RefuelDp, input) == ApiStatus::Ok, "live resource trace runs");
        Check(ApiLabRunInfo().source == LabDataSource::LiveRace, "resource trace declares the live planner origin");
        ArrayView<double> costs = ApiEdgeCosts(Preset::Balanced);
        GraphView graph = ApiGetGraph();
        for (int i = 0; i < graph.edges.count; ++i)
        {
            int id = graph.edges[i].id;
            Check(id >= 0 && id < costs.count, "cost labels index existing road IDs");
            if (id < 0 || id >= costs.count) continue;
            Check(graph.edges[i].status == EdgeStatus::Blocked ? costs[id] < 0 : costs[id] >= 0, "blocked and traversable road labels agree with live graph state");
        }
        Check(ApiLoadCircuit(0) == ApiStatus::Ok, "restore fresh circuit after presentation session");
    }

    void EventPlaybackContracts()
    {
        bool sawCancellation = false;
        for (int circuit = 0; circuit < ApiCircuitCount(); ++circuit)
        {
            Check(ApiLoadCircuit(circuit) == ApiStatus::Ok, "load circuit for event playback");
            Check(ApiLabRun(LabAlgo::EventQueue, ApiLabDefaultInput()) == ApiStatus::Ok, "record real circuit event trace");
            ArrayView<LabEventView> events = ApiLabEvents();
            int steps = ApiLabStepCount();
            Check(events.count > 0 && steps > 0, "event regression exercises a nonempty trace");
            const char** expected = new const char*[events.count];
            const char** history = new const char*[(steps + 1) * events.count];
            LabFrame initial = ApiLabFrame(-1);
            bool initialMatches = initial.heapPushes == 0;
            for (int i = 0; i < events.count; ++i)
            {
                expected[i] = "NOT YET SCHEDULED";
                history[i] = expected[i];
                initialMatches = initialMatches && strcmp(LabEventStatus::Label(initial, events[i]), expected[i]) == 0;
            }
            Check(initialMatches, "initial frame does not claim any future event is queued");
            long long pushes = 0;
            for (int step = 0; step < steps; ++step)
            {
                LabFrame frame = ApiLabFrame(step);
                if (frame.op == LabOp::PushHeap || frame.op == LabOp::Reject || frame.op == LabOp::PopHeap)
                {
                    bool found = false;
                    for (int i = 0; i < events.count; ++i)
                    {
                        if (events[i].id == frame.a)
                        {
                            found = true;
                            if (frame.op == LabOp::PushHeap)
                            {
                                Check(events[i].sequence == pushes, "recorded PushHeap maps to the next insertion sequence");
                                expected[i] = "QUEUED";
                            }
                            else if (frame.op == LabOp::Reject)
                            {
                                expected[i] = "CANCELLED";
                                sawCancellation = true;
                            }
                            else
                            {
                                expected[i] = "APPLIED";
                            }
                        }
                    }
                    Check(found, "event trace operation identifies an actual displayed event");
                }
                if (frame.op == LabOp::PushHeap)
                {
                    ++pushes;
                }
                bool matches = true;
                for (int i = 0; i < events.count; ++i)
                {
                    history[(step + 1) * events.count + i] = expected[i];
                    matches = matches && strcmp(LabEventStatus::Label(frame, events[i]), expected[i]) == 0;
                }
                Check(matches, "event labels match recorded scheduling, cancellation and application at each step");
            }
            bool complete = true;
            for (int i = 0; i < events.count; ++i)
            {
                complete = complete && (strcmp(expected[i], "APPLIED") == 0 || strcmp(expected[i], "CANCELLED") == 0);
            }
            Check(complete, "final event trace contains only applied or cancelled entries");
            for (int step = steps - 1; step >= -1; --step)
            {
                LabFrame frame = ApiLabFrame(step);
                bool matches = true;
                for (int i = 0; i < events.count; ++i)
                {
                    matches = matches && strcmp(LabEventStatus::Label(frame, events[i]), history[(step + 1) * events.count + i]) == 0;
                }
                Check(matches, "backward seek restores the recorded event labels including the initial frame");
            }
            delete[] history;
            delete[] expected;
        }
        Check(sawCancellation, "event playback regression includes real cancelled expiries");
        Check(ApiLoadCircuit(0) == ApiStatus::Ok, "restore Harbor after event playback checks");
    }

    void LabContracts()
    {
        LabInput input = ApiLabDefaultInput();
        input.seed = 2;
        input.useBriefExample = true;
        for (int module = 0; module < 13; ++module)
        {
            if (ApiLabRun(static_cast<LabAlgo>(module), input) != ApiStatus::Ok)
            {
                Check(false, ApiLastError());
                continue;
            }
            int steps = ApiLabStepCount();
            Check(steps > 0, "Lab module records actual operations");
            LabFrame end = ApiLabFrame(steps - 1);
            Check(end.step == steps - 1, "Lab can seek to final operation");
            LabFrame initial = ApiLabFrame(-1);
            Check(initial.step == -1 && initial.visited == 0 && initial.comparisons == 0, "backward seek restores initial counters");
        }
    }
}

int main()
{
    FormatAndClock();
    ExplorerContracts();
    RaceAndChampionship();
    LabContracts();
    V11PresentationContracts();
    EventPlaybackContracts();
    ApiShutdown();
    printf("GUI contract checks: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
