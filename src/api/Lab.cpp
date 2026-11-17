// The Algorithm Lab (Screen 4) and the track generator's API.
//
// Every module here calls the SAME function the game uses -- algo::Bfs,
// algo::Dijkstra, algo::KnapsackDp, algo::MergeSort, sim::EventQueue, ... --
// once, with an algo::Trace attached; algo::TraceCursor then rebuilds the
// visible state after any recorded step. Nothing is re-implemented for
// display, which is the brief's requirement that "the Algorithm Lab must
// call the same algorithm implementation used by gameplay".
#include "SmartRaceApi.h"
#include "ApiInternal.h"
#include "../algo/Bfs.h"
#include "../algo/Dfs.h"
#include "../algo/Dijkstra.h"
#include "../algo/AStar.h"
#include "../algo/Kruskal.h"
#include "../algo/Knapsack.h"
#include "../algo/RefuelDp.h"
#include "../algo/Sorting.h"
#include "../algo/Comparators.h"
#include "../algo/Pseudocode.h"
#include "../algo/Trace.h"
#include "../algo/TraceCursor.h"
#include "../sim/TrackGenerator.h"
#include "../sim/ResourcePlanner.h"
#include "../sim/EventQueue.h"
#include "../io/TrackWriter.h"
#include "../model/Rules.h"
#include "../core/util/Rng.h"
#include "../core/util/String.h"

#include <cstdio>
#include <cstdarg>

namespace
{
    bool g_labReady = false;
    algo::Trace g_labTrace;
    algo::TraceCursor g_labCursor;
    char g_labResult[220] = "";

    // v1.1 (H8): where the last run's data came from.
    LabRunInfo g_labInfo = { LabDataSource::None, "", -1, -1 };
    char g_labInfoText[200] = "";

    void SetInfo(LabDataSource source, int sourceVertex, int targetVertex, const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        vsnprintf(g_labInfoText, sizeof(g_labInfoText), fmt, args);
        va_end(args);
        g_labInfo.source = source;
        g_labInfo.description = g_labInfoText;
        g_labInfo.sourceVertex = sourceVertex;
        g_labInfo.targetVertex = targetVertex;
    }

    // The track itself: live while a race runs (blocks, oil, outages).
    void SetTrackInfo(int sourceVertex, int targetVertex)
    {
        if (api_internal::RaceRunning())
        {
            SetInfo(LabDataSource::LiveRace, sourceVertex, targetVertex,
                "Live race track at %.1f s (current blocks and effects)",
                api_internal::HumanPlannerState().raceTimeMs / 1000.0);
        }
        else
        {
            SetInfo(LabDataSource::LoadedCircuit, sourceVertex, targetVertex, "Loaded circuit (no race running)");
        }
    }

    // H7: where pickups/refuelling are planned from.
    api_internal::PlannerState PlannerFor(const LabInput& input)
    {
        if (input.useRaceState && api_internal::RaceRunning()) return api_internal::HumanPlannerState();
        return api_internal::BaselinePlannerState();
    }

    void SetPlannerInfo(const api_internal::PlannerState& state, bool requestedLive)
    {
        if (state.live)
        {
            SetInfo(LabDataSource::LiveRace, state.vertex, -1,
                "Live race at %.1f s: human at V%d, %.1f fuel units, %d nitro, %d more full lap(s)",
                state.raceTimeMs / 1000.0, state.vertex, state.fuelTenths / 10.0, state.nitroCharges, state.extraLaps);
        }
        else
        {
            SetInfo(LabDataSource::LoadedCircuit, state.vertex, -1,
                "Pre-race baseline: Start V%d, full tank, %d nitro, whole race%s",
                state.vertex, state.nitroCharges, requestedLive ? " (live state requested, but no race is running)" : "");
        }
    }

    // The generator's candidate-road graph: Graph frames of a generator run
    // index into it rather than into the loaded circuit.
    model::Graph* g_labCandidateGraph = nullptr;
    bool g_labUsesOwnGraph = false;
    DynArray<VertexView> g_labVertexViews;
    DynArray<EdgeView> g_labEdgeViews;

    io::TrackData g_generatedTrack;
    bool g_hasGenerated = false;
    sim::GenReport g_genReport;
    DynArray<GeneratorAttemptView> g_genAttemptViews;

    DynArray<LabEventView> g_labEvents;

    struct IntKeyOrder
    {
        static int Compare(const int& a, const int& b)
        {
            if (a < b) return -1;
            if (a > b) return 1;
            return 0;
        }

        static int Key(const int& v)
        {
            return v;
        }
    };

    void SetResult(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        vsnprintf(g_labResult, sizeof(g_labResult), fmt, args);
        va_end(args);
    }

    void Ready(algo::FrameKind kind, int vertexCount, int edgeCount, const int* initialArray, int arrayLength,
        int tableRows, int tableCols)
    {
        g_labCursor.Reset(&g_labTrace, kind, vertexCount, edgeCount, initialArray, arrayLength, tableRows, tableCols);
        g_labReady = true;
    }

    algo::PseudocodeId ToPseudocodeId(LabAlgo algo)
    {
        switch (algo)
        {
            case LabAlgo::Bfs: return algo::PseudocodeId::Bfs;
            case LabAlgo::Dfs: return algo::PseudocodeId::Dfs;
            case LabAlgo::Dijkstra: return algo::PseudocodeId::Dijkstra;
            case LabAlgo::AStar: return algo::PseudocodeId::AStar;
            case LabAlgo::Kruskal: return algo::PseudocodeId::Kruskal;
            case LabAlgo::TrackGenerator: return algo::PseudocodeId::TrackGenerator;
            case LabAlgo::KnapsackDp: return algo::PseudocodeId::KnapsackDp;
            case LabAlgo::GreedyPickup: return algo::PseudocodeId::GreedyPickup;
            case LabAlgo::RefuelDp: return algo::PseudocodeId::RefuelDp;
            case LabAlgo::MergeSort: return algo::PseudocodeId::MergeSort;
            case LabAlgo::QuickSort: return algo::PseudocodeId::QuickSort;
            case LabAlgo::HeapSort: return algo::PseudocodeId::HeapSort;
            case LabAlgo::EventQueue:
            default: return algo::PseudocodeId::EventQueue;
        }
    }

    LabFrameKind ToLabKind(algo::FrameKind kind)
    {
        switch (kind)
        {
            case algo::FrameKind::Array: return LabFrameKind::Array;
            case algo::FrameKind::Table: return LabFrameKind::Table;
            case algo::FrameKind::Events: return LabFrameKind::Events;
            case algo::FrameKind::Graph:
            default: return LabFrameKind::Graph;
        }
    }

    // ---- Graph searches ----------------------------------------------

    // The mid-lap checkpoint (CP5 on the shipped circuits): the Finish sits
    // one road from the Start on a lap circuit, which makes a poor demo.
    int DefaultTarget(const model::Graph& g)
    {
        int highest = 0;
        for (int v = 0; v < g.VertexCount(); ++v)
        {
            if (g.GetVertex(v).checkpointOrder > highest) highest = g.GetVertex(v).checkpointOrder;
        }
        int wanted = (highest + 1) / 2;
        for (int v = 0; v < g.VertexCount(); ++v)
        {
            if (wanted > 0 && g.GetVertex(v).checkpointOrder == wanted) return v;
        }
        return api_internal::FinishVertex();
    }

    ApiStatus RunGraphSearch(LabAlgo algo, const LabInput& input)
    {
        const model::Graph& g = *api_internal::CurrentGraph();
        int n = g.VertexCount();
        int source = (input.source >= 0) ? input.source : api_internal::StartVertex();
        int target = (input.target >= 0) ? input.target : DefaultTarget(g);
        if (source < 0 || source >= n || target < 0 || target >= n)
        {
            api_internal::SetError("source/target is not a vertex of this circuit");
            return ApiStatus::InvalidId;
        }

        model::Weights weights = model::WeightsForPreset(api_internal::ToModelPreset(input.preset));
        const model::CostReferences& refs = api_internal::CostRefs();
        algo::AlgoStats stats;

        switch (algo)
        {
            case LabAlgo::Bfs:
            {
                algo::BfsResult r = algo::Bfs(g, source, target, &stats, &g_labTrace);
                if (r.found)
                {
                    SetResult("BFS: V%d -> V%d in %d roads (fewest roads, not least distance)", source, target, r.distEdges[target]);
                }
                else
                {
                    SetResult("BFS: V%d cannot reach V%d", source, target);
                }
                break;
            }
            case LabAlgo::Dfs:
            {
                algo::DfsTraversalResult r = algo::DfsTraversal(g, source, &stats, &g_labTrace);
                SetResult("DFS from V%d reached %d of %d vertices", source, r.order.Size(), n);
                break;
            }
            case LabAlgo::Dijkstra:
            {
                algo::PathResult r = algo::Dijkstra(g, source, target, weights, refs, &stats, &g_labTrace);
                if (r.found)
                {
                    SetResult("Dijkstra: cost %.3f over %d roads; %lld vertices expanded, %lld relaxations, %lld heap pushes",
                        r.totalCost, r.edgePath.Size(), stats.verticesVisited, stats.edgesRelaxed, stats.heapPushes);
                }
                else
                {
                    SetResult("Dijkstra: V%d cannot reach V%d", source, target);
                }
                break;
            }
            case LabAlgo::AStar:
            default:
            {
                double scale = algo::ComputeHeuristicScale(g, weights, refs);
                algo::PathResult r = algo::AStar(g, source, target, weights, refs, scale, &stats, &g_labTrace);
                if (r.found)
                {
                    SetResult("A*: cost %.3f over %d roads; %lld vertices expanded, %lld relaxations (h scale %.4f)",
                        r.totalCost, r.edgePath.Size(), stats.verticesVisited, stats.edgesRelaxed, scale);
                }
                else
                {
                    SetResult("A*: V%d cannot reach V%d", source, target);
                }
                break;
            }
        }

        SetTrackInfo(source, (algo == LabAlgo::Dfs) ? -1 : target);
        Ready(algo::FrameKind::Graph, n, g.EdgeCount(), nullptr, 0, 0, 0);
        return ApiStatus::Ok;
    }

    // ---- Kruskal on the loaded circuit -------------------------------

    ApiStatus RunKruskal()
    {
        const model::Graph& g = *api_internal::CurrentGraph();
        DynArray<algo::CandidateEdge> candidates;
        for (int e = 0; e < g.EdgeCount(); ++e)
        {
            const model::Edge& edge = g.GetEdge(e);
            algo::CandidateEdge c;
            c.id = e;
            c.from = edge.from;
            c.to = edge.to;
            c.weight = edge.distance;
            candidates.Push(c);
        }

        algo::MstResult r = algo::Kruskal(g.VertexCount(), candidates.Data(), candidates.Size(), nullptr, &g_labTrace);
        SetResult("Kruskal: %d roads kept (total length %.0f), %d rejected as cycle-closing, %d component(s)",
            r.acceptedIds.Size(), r.totalWeight, r.rejectedIds.Size(), r.componentCount);
        SetTrackInfo(-1, -1);
        Ready(algo::FrameKind::Graph, g.VertexCount(), g.EdgeCount(), nullptr, 0, 0, 0);
        return ApiStatus::Ok;
    }

    // ---- Track generator ---------------------------------------------

    ApiStatus RunGenerator(const LabInput& input)
    {
        sim::GeneratorParams params;
        params.seed = input.seed;
        params.vertexCount = input.generatorVertexCount;
        params.laps = api_internal::Laps();

        delete g_labCandidateGraph;
        g_labCandidateGraph = nullptr;
        bool ok = sim::GenerateTrack(params, &g_generatedTrack, &g_labCandidateGraph, &g_genReport, &g_labTrace);
        g_hasGenerated = ok;

        int vertices = 0;
        int edges = 0;
        if (g_labCandidateGraph != nullptr)
        {
            api_internal::BuildGraphViews(*g_labCandidateGraph, &g_labVertexViews, &g_labEdgeViews);
            g_labUsesOwnGraph = true;
            vertices = g_labCandidateGraph->VertexCount();
            edges = g_labCandidateGraph->EdgeCount();
        }

        int attempts = g_genReport.attempts.Size();
        if (ok)
        {
            SetResult("Generator: attempt %d of %d accepted (%d rejected first): %d vertices, %d roads",
                g_genReport.acceptedAttempt + 1, attempts, g_genReport.acceptedAttempt,
                g_generatedTrack.graph->VertexCount(), g_generatedTrack.graph->EdgeCount());
        }
        else
        {
            SetResult("Generator: all %d attempts rejected; last: %s", attempts,
                attempts > 0 ? g_genReport.attempts[attempts - 1].reason : "?");
        }

        SetInfo(LabDataSource::GeneratedTrack, -1, -1, "Track generator, seed %llu, %d vertices (candidate roads of %s)",
            input.seed, input.generatorVertexCount, ok ? "the accepted attempt" : "the last attempt");
        Ready(algo::FrameKind::Graph, vertices, edges, nullptr, 0, 0, 0);
        return ApiStatus::Ok;
    }

    // ---- Knapsack DP / greedy ----------------------------------------

    algo::PickupItem Item(int id, int cost, int benefit)
    {
        algo::PickupItem item;
        item.id = id;
        item.vertex = -1;
        item.cost = cost;
        item.benefit = benefit;
        return item;
    }

    // The pickups the planner would offer -- from the pre-race baseline, or
    // (useRaceState) from the human's live state, exactly as ApiPlanPickups
    // computes them -- or the brief's A/B/C example when asked for, or when
    // the circuit offers none.
    void BuildLabItems(const LabInput& input, DynArray<algo::PickupItem>* items, int* budget, bool* usedExample)
    {
        items->Clear();
        *usedExample = true;

        if (!input.useBriefExample && api_internal::CircuitLoaded())
        {
            const model::Graph& g = *api_internal::CurrentGraph();
            model::Weights weights = model::WeightsForPreset(api_internal::ToModelPreset(input.preset));
            int finish = api_internal::FinishVertex();
            api_internal::PlannerState state = PlannerFor(input);

            DynArray<int> raceV;
            DynArray<int> raceE;
            bool fuelNeeded = false;
            if (sim::BuildRaceRoute(g, state.vertex, state.lastCompletedCheckpoint, state.extraLaps, finish, weights,
                api_internal::CostRefs(), &raceV, &raceE))
            {
                algo::RefuelProblem problem;
                sim::BuildRefuelProblem(g, raceV, raceE, state.fuelTenths, &problem);
                fuelNeeded = !algo::PlanRefuelling(problem).feasible;
            }

            DynArray<int> lapV;
            DynArray<int> lapE;
            DynArray<sim::PickupInfo> info;
            if (sim::BuildRaceRoute(g, state.vertex, state.lastCompletedCheckpoint, 0, finish, weights,
                api_internal::CostRefs(), &lapV, &lapE))
            {
                sim::BuildPickupItems(g, lapV, lapE, state.nitroCharges, fuelNeeded, items, &info);
            }

            if (items->Size() > 0)
            {
                SetPlannerInfo(state, input.useRaceState);
                *usedExample = false;
                *budget = input.budgetTenthsSec;
                if (*budget < 0) *budget = 0;
                if (*budget > 2000) *budget = 2000;   // keeps the table a sensible size to display
                return;
            }
        }

        items->Push(Item(0, 6, 30));
        items->Push(Item(1, 5, 24));
        items->Push(Item(2, 5, 24));
        *budget = 10;
        if (input.useBriefExample)
        {
            SetInfo(LabDataSource::BriefExample, -1, -1, "The brief's A(6, 30), B(5, 24), C(5, 24) example, budget 10");
        }
        else
        {
            SetInfo(LabDataSource::BriefExampleFallback, -1, -1,
                "No pickups are on offer from here, so the brief's A/B/C example (budget 10) was used instead");
        }
    }

    ApiStatus RunKnapsack(LabAlgo algo, const LabInput& input)
    {
        DynArray<algo::PickupItem> items;
        int budget = 0;
        bool example = true;
        BuildLabItems(input, &items, &budget, &example);
        const char* source = example ? "the brief's A/B/C example" : "this circuit's pickups";

        if (algo == LabAlgo::KnapsackDp)
        {
            algo::AlgoStats stats;
            algo::KnapsackResult r = algo::KnapsackDp(items.Data(), items.Size(), budget, &stats, &g_labTrace);
            SetResult("Knapsack DP on %s: benefit %d for cost %d of budget %d (%d items, %lld cells)",
                source, r.totalBenefit, r.totalCost, budget, r.selectedIds.Size(), stats.dpStates);
            Ready(algo::FrameKind::Table, 0, 0, nullptr, 0, items.Size() + 1, budget + 1);
        }
        else
        {
            algo::KnapsackResult r = algo::GreedyPickup(items.Data(), items.Size(), budget, nullptr, &g_labTrace);
            SetResult("Greedy on %s: benefit %d for cost %d of budget %d (%d items; not guaranteed optimal)",
                source, r.totalBenefit, r.totalCost, budget, r.selectedIds.Size());
            Ready(algo::FrameKind::Table, 0, 0, nullptr, 0, 0, 0);
        }
        return ApiStatus::Ok;
    }

    // ---- Refuelling DP -----------------------------------------------

    ApiStatus RunRefuel(const LabInput& input)
    {
        const model::Graph& g = *api_internal::CurrentGraph();
        model::Weights weights = model::WeightsForPreset(api_internal::ToModelPreset(input.preset));
        int start = api_internal::StartVertex();
        int finish = api_internal::FinishVertex();
        int laps = api_internal::Laps();
        api_internal::PlannerState state = PlannerFor(input);
        SetPlannerInfo(state, input.useRaceState);

        // Live (H7): exactly ApiPlanRefuel's problem -- the rest of the race
        // from the human's position with the human's fuel.
        DynArray<int> vertices;
        DynArray<int> edges;
        bool built = false;
        if (state.live)
        {
            if (!sim::BuildRaceRoute(g, state.vertex, state.lastCompletedCheckpoint, state.extraLaps, finish, weights,
                api_internal::CostRefs(), &vertices, &edges))
            {
                api_internal::SetError("no route from the human's current position");
                return ApiStatus::Infeasible;
            }
            built = true;
        }

        // Baseline: a race route that passes the first active station every
        // lap gives the DP real choices; fall back to the plain route.
        for (int s = 0; s < g.VertexCount() && !built; ++s)
        {
            const model::Vertex& v = g.GetVertex(s);
            if (v.type != model::VertexType::FuelStation || !v.active) continue;

            vertices.Clear();
            edges.Clear();
            built = sim::AppendLapVia(g, start, 0, s, finish, weights, api_internal::CostRefs(), &vertices, &edges);
            for (int lap = 1; lap < laps && built; ++lap)
            {
                built = sim::AppendLapVia(g, finish, 0, s, finish, weights, api_internal::CostRefs(), &vertices, &edges);
            }
        }
        if (!built && !sim::BuildRaceRoute(g, start, 0, laps - 1, finish, weights, api_internal::CostRefs(), &vertices, &edges))
        {
            api_internal::SetError("no route around this circuit");
            return ApiStatus::Infeasible;
        }

        algo::RefuelProblem problem;
        sim::BuildRefuelProblem(g, vertices, edges, state.fuelTenths, &problem);
        algo::RefuelPlan plan = algo::PlanRefuelling(problem, nullptr, &g_labTrace);

        if (plan.feasible)
        {
            SetResult("Refuel DP over %d stops: %d purchase(s), %.1fs stopped, finishing with %.1f units",
                problem.legFuel.Size(), plan.stopCount, plan.totalStopTimeMs / 1000.0, plan.finalFuel / 10.0);
        }
        else
        {
            SetResult("Refuel DP over %d stops: no plan finishes the race on this route", problem.legFuel.Size());
        }
        Ready(algo::FrameKind::Table, 0, 0, nullptr, 0, problem.legFuel.Size() + 1, problem.capacity + 1);
        return ApiStatus::Ok;
    }

    // ---- Sorts --------------------------------------------------------

    ApiStatus RunSort(LabAlgo algo, const LabInput& input)
    {
        algo::AlgoStats stats;
        DynArray<int> initialKeys;

        if (api_internal::ChampionshipActive() && api_internal::Championship().EntryCount() >= 2)
        {
            // The live standings, unsorted (racer-id order), ranked by the
            // championship comparator. Keys are racer ids.
            const DynArray<model::ChampionshipEntry>& entries = api_internal::Championship().Entries();
            DynArray<model::ChampionshipEntry> work;
            for (int i = 0; i < entries.Size(); ++i)
            {
                work.Push(entries[i]);
                initialKeys.Push(entries[i].racerId);
            }

            if (algo == LabAlgo::MergeSort)
            {
                algo::MergeSort<model::ChampionshipEntry, algo::StandingOrder>(work.Data(), work.Size(), &stats, &g_labTrace);
            }
            else if (algo == LabAlgo::QuickSort)
            {
                algo::QuickSort<model::ChampionshipEntry, algo::StandingOrder>(work.Data(), work.Size(), &stats, &g_labTrace);
            }
            else
            {
                algo::HeapSort<model::ChampionshipEntry, algo::StandingOrder>(work.Data(), work.Size(), &stats, &g_labTrace);
            }
            SetInfo(LabDataSource::ChampionshipStandings, -1, -1, "Live championship standings after %d race(s)",
                api_internal::Championship().RacesCompleted());
            SetResult("Ranked the %d championship entries (keys are racer ids): %lld comparisons, %lld moves/swaps",
                work.Size(), stats.comparisons, stats.swaps);
        }
        else
        {
            int n = input.arraySize;
            if (n < 2) n = 2;
            if (n > 64) n = 64;

            Rng rng(input.seed);
            DynArray<int> work;
            for (int i = 0; i < n; ++i)
            {
                int key = rng.NextInt(0, 99);
                work.Push(key);
                initialKeys.Push(key);
            }

            if (algo == LabAlgo::MergeSort)
            {
                algo::MergeSort<int, IntKeyOrder>(work.Data(), n, &stats, &g_labTrace);
            }
            else if (algo == LabAlgo::QuickSort)
            {
                algo::QuickSort<int, IntKeyOrder>(work.Data(), n, &stats, &g_labTrace);
            }
            else
            {
                algo::HeapSort<int, IntKeyOrder>(work.Data(), n, &stats, &g_labTrace);
            }
            SetInfo(LabDataSource::RandomKeys, -1, -1, "%d random keys 0..99 from seed %llu (no championship running)",
                n, input.seed);
            SetResult("Sorted %d random keys (no championship running): %lld comparisons, %lld moves/swaps",
                n, stats.comparisons, stats.swaps);
        }

        Ready(algo::FrameKind::Array, 0, 0, initialKeys.Data(), initialKeys.Size(), 0, 0);
        return ApiStatus::Ok;
    }

    // ---- Event queue -------------------------------------------------

    bool ParseScriptedType(const char* s, sim::EventType* out)
    {
        static const char* const kNames[7] =
        {
            "RoadBlock", "RoadClear", "ShortcutToggle", "OilSpill", "SpeedRestriction", "FuelStationToggle", "NitroBooster",
        };
        static const sim::EventType kTypes[7] =
        {
            sim::EventType::RoadBlock, sim::EventType::RoadClear, sim::EventType::ShortcutToggle, sim::EventType::OilSpill,
            sim::EventType::SpeedRestriction, sim::EventType::FuelStationToggle, sim::EventType::NitroBooster,
        };
        for (int i = 0; i < 7; ++i)
        {
            if (String::StrEquals(s, kNames[i]))
            {
                *out = kTypes[i];
                return true;
            }
        }
        return false;
    }

    const char* EventTypeName(sim::EventType type)
    {
        switch (type)
        {
            case sim::EventType::RoadBlock: return "RoadBlock";
            case sim::EventType::RoadClear: return "RoadClear";
            case sim::EventType::ShortcutToggle: return "ShortcutToggle";
            case sim::EventType::OilSpill: return "OilSpill";
            case sim::EventType::OilSpillExpire: return "OilSpillExpire";
            case sim::EventType::SpeedRestriction: return "SpeedRestriction";
            case sim::EventType::SpeedRestrictionExpire: return "SpeedRestrictionExpire";
            case sim::EventType::FuelStationToggle: return "FuelStationToggle";
            case sim::EventType::FuelStationReactivate: return "FuelStationReactivate";
            case sim::EventType::NitroBooster: return "NitroBooster";
            case sim::EventType::NitroBoosterExpire: return "NitroBoosterExpire";
            default: return "?";
        }
    }

    bool ExpiryFor(sim::EventType type, sim::EventType* expiry)
    {
        switch (type)
        {
            case sim::EventType::OilSpill:
            {
                *expiry = sim::EventType::OilSpillExpire;
                return true;
            }
            case sim::EventType::SpeedRestriction:
            {
                *expiry = sim::EventType::SpeedRestrictionExpire;
                return true;
            }
            case sim::EventType::FuelStationToggle:
            {
                *expiry = sim::EventType::FuelStationReactivate;
                return true;
            }
            case sim::EventType::NitroBooster:
            {
                *expiry = sim::EventType::NitroBoosterExpire;
                return true;
            }
            default:
            {
                return false;
            }
        }
    }

    // Records what the queue hands back, in the order it hands it back.
    struct RecordingSink : sim::IEventSink
    {
        RecordingSink()
            : applied(0)
        {
        }

        void OnEvent(const sim::Event& e) override
        {
            ++applied;
            g_labTrace.Record(algo::TraceOp::PopHeap, e.eventId, static_cast<int>(e.type),
                static_cast<double>(e.triggerTimeMs), algo::lines::events::kPop);
        }

        int applied;
    };

    void AddEventView(const sim::EventQueue& queue, int id)
    {
        const sim::Event* e = queue.Find(id);
        if (e == nullptr) return;

        LabEventView view;
        view.id = e->eventId;
        view.type = EventTypeName(e->type);
        view.triggerTimeMs = e->triggerTimeMs;
        view.priority = e->priority;
        view.sequence = e->insertionSequence;
        view.target = e->target;
        g_labEvents.Push(view);
    }

    ApiStatus RunEventQueue()
    {
        const io::TrackData& track = api_internal::CurrentTrack();
        sim::EventQueue queue;
        DynArray<int> boosterExpiries;

        // Schedule the circuit's scripted events plus the expiry each timed
        // effect schedules for itself in a race (RaceEngine::OnEvent).
        for (int i = 0; i < track.events.Size(); ++i)
        {
            const io::ScriptedEvent& se = track.events[i];
            sim::EventType type;
            if (!ParseScriptedType(se.type.CStr(), &type)) continue;

            int id = queue.Schedule(type, se.triggerTimeMs, se.target, se.durationMs);
            g_labTrace.Record(algo::TraceOp::PushHeap, id, static_cast<int>(type), se.triggerTimeMs, algo::lines::events::kSchedule);
            AddEventView(queue, id);

            sim::EventType expiry;
            if (ExpiryFor(type, &expiry))
            {
                int expiryId = queue.Schedule(expiry, se.triggerTimeMs + se.durationMs, se.target, 0);
                g_labTrace.Record(algo::TraceOp::PushHeap, expiryId, static_cast<int>(expiry),
                    se.triggerTimeMs + se.durationMs, algo::lines::events::kSchedule);
                AddEventView(queue, expiryId);
                if (type == sim::EventType::NitroBooster) boosterExpiries.Push(expiryId);
            }
        }

        // A racer collecting a nitro boost cancels its expiry: lazy
        // deletion -- the entry stays in the heap and is skipped on pop.
        for (int i = 0; i < boosterExpiries.Size(); ++i)
        {
            queue.Cancel(boosterExpiries[i]);
            g_labTrace.Record(algo::TraceOp::Reject, boosterExpiries[i], -1, 0.0, algo::lines::events::kCancel);
        }

        RecordingSink sink;
        queue.ProcessUntil(0x7fffffff, &sink);

        SetResult("Event queue: %d events scheduled, %d applied in (time, priority, sequence) order, %d cancelled",
            g_labEvents.Size(), sink.applied, boosterExpiries.Size());
        SetInfo(LabDataSource::CircuitEvents, -1, -1, "The circuit's %d scripted event(s) plus their expiries",
            track.events.Size());
        Ready(algo::FrameKind::Events, 0, 0, nullptr, 0, 0, 0);
        return ApiStatus::Ok;
    }
}

namespace api_internal
{
    void ResetLab()
    {
        g_labReady = false;
        g_labTrace.Clear();
        g_labCursor.Reset(&g_labTrace, algo::FrameKind::Graph, 0, 0, nullptr, 0, 0, 0);
        g_labUsesOwnGraph = false;
        g_labEvents.Clear();
        g_labResult[0] = '\0';
        SetInfo(LabDataSource::None, -1, -1, "");
    }

    void ShutdownLab()
    {
        ResetLab();
        delete g_labCandidateGraph;
        g_labCandidateGraph = nullptr;
        g_hasGenerated = false;
        g_genReport.attempts.Clear();
        g_genReport.acceptedAttempt = -1;
        g_genAttemptViews.Clear();
    }
}

LabInput ApiLabDefaultInput()
{
    LabInput input;
    input.source = -1;
    input.target = -1;
    input.preset = Preset::Balanced;
    input.budgetTenthsSec = Rules::kKnapsackDefaultBudgetTenthsSec;
    input.useBriefExample = false;
    input.arraySize = 12;
    input.seed = 1;
    input.generatorVertexCount = 28;
    input.useRaceState = false;
    return input;
}

ApiStatus ApiLabRun(LabAlgo algo, const LabInput& input)
{
    api_internal::ResetLab();

    bool standalone = (algo == LabAlgo::MergeSort || algo == LabAlgo::QuickSort || algo == LabAlgo::HeapSort
        || algo == LabAlgo::TrackGenerator
        || ((algo == LabAlgo::KnapsackDp || algo == LabAlgo::GreedyPickup) && input.useBriefExample));
    if (!standalone && !api_internal::CircuitLoaded())
    {
        api_internal::SetError("this Lab module runs on the loaded circuit: load one first");
        return ApiStatus::NotLoaded;
    }

    switch (algo)
    {
        case LabAlgo::Bfs:
        case LabAlgo::Dfs:
        case LabAlgo::Dijkstra:
        case LabAlgo::AStar:
            return RunGraphSearch(algo, input);
        case LabAlgo::Kruskal:
            return RunKruskal();
        case LabAlgo::TrackGenerator:
            return RunGenerator(input);
        case LabAlgo::KnapsackDp:
        case LabAlgo::GreedyPickup:
            return RunKnapsack(algo, input);
        case LabAlgo::RefuelDp:
            return RunRefuel(input);
        case LabAlgo::MergeSort:
        case LabAlgo::QuickSort:
        case LabAlgo::HeapSort:
            return RunSort(algo, input);
        case LabAlgo::EventQueue:
            return RunEventQueue();
        default:
        {
            api_internal::SetError("unknown Lab module");
            return ApiStatus::InvalidArgument;
        }
    }
}

int ApiLabStepCount()
{
    return g_labReady ? g_labTrace.StepCount() : 0;
}

LabFrame ApiLabFrame(int step)
{
    LabFrame f;
    f.step = -1;
    f.stepCount = 0;
    f.kind = LabFrameKind::Graph;
    f.op = LabOp::VisitVertex;
    f.a = -1;
    f.b = -1;
    f.value = 0.0;
    f.line = -1;

    if (g_labReady) g_labCursor.SeekTo(step);

    const algo::TraceCursor& c = g_labCursor;
    f.step = c.CurrentStep();
    f.stepCount = ApiLabStepCount();
    f.kind = ToLabKind(c.Kind());
    if (g_labReady && f.step >= 0)
    {
        const algo::TraceStep& s = g_labTrace.Step(f.step);
        f.op = static_cast<LabOp>(s.op);
        f.a = s.a;
        f.b = s.b;
        f.value = s.value;
        f.line = s.line;
    }

    f.vertexState.data = c.VertexState().Data();
    f.vertexState.count = c.VertexState().Size();
    f.vertexValue.data = c.VertexValue().Data();
    f.vertexValue.count = c.VertexValue().Size();
    f.vertexParent.data = c.VertexParent().Data();
    f.vertexParent.count = c.VertexParent().Size();
    f.edgeState.data = c.EdgeState().Data();
    f.edgeState.count = c.EdgeState().Size();
    f.arrayValues.data = c.ArrayValues().Data();
    f.arrayValues.count = c.ArrayValues().Size();
    f.tableRows = c.TableRows();
    f.tableCols = c.TableCols();
    f.tableCells.data = c.Table().Data();
    f.tableCells.count = c.Table().Size();
    f.selectedIds.data = c.Selected().Data();
    f.selectedIds.count = c.Selected().Size();
    f.rejectedIds.data = c.Rejected().Data();
    f.rejectedIds.count = c.Rejected().Size();

    const algo::FrameCounters& k = c.Counters();
    f.visited = k.visited;
    f.examined = k.examined;
    f.relaxed = k.relaxed;
    f.comparisons = k.comparisons;
    f.swaps = k.swaps;
    f.tableCellsFilled = k.tableCells;
    f.heapPushes = k.heapPushes;
    f.heapPops = k.heapPops;
    f.placements = k.placements;
    f.backtracks = k.backtracks;
    f.rejections = k.rejections;
    return f;
}

GraphView ApiLabGraph()
{
    if (!g_labUsesOwnGraph) return ApiGetGraph();

    GraphView view;
    view.vertices.data = g_labVertexViews.Data();
    view.vertices.count = g_labVertexViews.Size();
    view.edges.data = g_labEdgeViews.Data();
    view.edges.count = g_labEdgeViews.Size();
    return view;
}

PseudocodeView ApiLabPseudocode(LabAlgo algo)
{
    PseudocodeView view;
    const char* const* lines = nullptr;
    int count = algo::GetPseudocode(ToPseudocodeId(algo), &lines);
    view.lines.data = lines;
    view.lines.count = count;
    return view;
}

LabRunInfo ApiLabRunInfo()
{
    g_labInfo.description = g_labInfoText;
    return g_labInfo;
}

const char* ApiLabResultText()
{
    return g_labResult;
}

ArrayView<LabEventView> ApiLabEvents()
{
    ArrayView<LabEventView> view;
    view.data = g_labEvents.Data();
    view.count = g_labEvents.Size();
    return view;
}

GeneratorReportView ApiLabGeneratorReport()
{
    g_genAttemptViews.Clear();
    for (int i = 0; i < g_genReport.attempts.Size(); ++i)
    {
        const sim::GenAttempt& a = g_genReport.attempts[i];
        GeneratorAttemptView view;
        view.attempt = a.attempt;
        view.accepted = a.accepted;
        view.reason = a.reason;
        view.nodesExplored = a.nodesExplored;
        view.prunedBranches = a.prunedBranches;
        g_genAttemptViews.Push(view);
    }

    GeneratorReportView report;
    report.attempts.data = g_genAttemptViews.Data();
    report.attempts.count = g_genAttemptViews.Size();
    report.acceptedAttempt = g_genReport.acceptedAttempt;
    return report;
}

ApiStatus ApiLabSaveGenerated(const char* path)
{
    if (!g_hasGenerated || g_generatedTrack.graph == nullptr)
    {
        api_internal::SetError("no generated track yet: run the TrackGenerator Lab module first");
        return ApiStatus::NotLoaded;
    }

    char message[160];
    if (!io::WriteTrackFile(path, g_generatedTrack, message, sizeof(message)))
    {
        api_internal::SetError(message);
        return ApiStatus::IoError;
    }
    return ApiStatus::Ok;
}
