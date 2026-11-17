// Experiment 2 -- Dijkstra and A* (Project.pdf section 7).
//
// Inputs:
//   * the 3 real circuits (data/circuits), queries Start -> CP5 and
//     CP5 -> CP10 (each a leg the race planner really solves);
//   * jittered-grid road networks with V in {1k, 5k, 10k, 50k, 100k}
//     (bench/GraphGen.cpp documents coordinates and costs), queries
//     "far" (top-left corner -> bottom-right corner) and "near" (centre ->
//     a vertex ~10% of the grid away);
//   * all 4 presets (Balanced, Speed, Economy, Safety), plus the pure
//     Distance weighting as a control: its heuristic is tight, while the
//     game presets' weighted costs make straight-line distance a weak bound.
// Timed: the search only. The cost references (normalisation) and A*'s
// heuristic scale (one O(E) pass, minimum cost/straight-line ratio) are
// computed once per graph and preset outside the timed region; the scale's
// own cost is reported as the separate "AStar_scale" row. Each search call
// includes initialising its own dist/parent arrays and heap.
//
// Correctness (untimed): A* cost == Dijkstra cost (relative error < 1e-9)
// for every query; both find a path whenever the other does.
#include "BenchUtil.h"
#include "GraphGen.h"
#include "../src/algo/Dijkstra.h"
#include "../src/algo/AStar.h"
#include "../src/io/TrackLoader.h"
#include "../src/model/CostModel.h"

#include <cmath>
#include <cstdio>

namespace
{
    // The 4 game presets, plus the pure-distance weighting (Opponent 3's
    // metric) as a control where the straight-line heuristic is tight.
    const int kPresetCount = 5;
    const model::Preset kPresets[kPresetCount] = { model::Preset::Balanced, model::Preset::Speed, model::Preset::Economy,
        model::Preset::Safety, model::Preset::Distance };
    const char* const kPresetNames[kPresetCount] = { "Balanced", "Speed", "Economy", "Safety", "Distance" };

    int CheckpointVertex(const model::Graph& g, int order)
    {
        for (int v = 0; v < g.VertexCount(); ++v)
        {
            if (g.GetVertex(v).checkpointOrder == order) return v;
        }
        return -1;
    }

    int VertexOfType(const model::Graph& g, model::VertexType type)
    {
        for (int v = 0; v < g.VertexCount(); ++v)
        {
            if (g.GetVertex(v).type == type) return v;
        }
        return -1;
    }

    // Times Dijkstra and A* on one query under every preset.
    void RunQueries(const BenchConfig& cfg, ExperimentOutput& out, const model::Graph& g, const char* graphName,
        const char* queryName, int source, int target, int reps)
    {
        double samples[kMaxRuns];
        char config[200];
        char counters[160];
        model::CostReferences refs = model::ComputeReferences(g);

        for (int p = 0; p < kPresetCount; ++p)
        {
            model::Weights w = model::WeightsForPreset(kPresets[p]);

            double scale = 0.0;
            {
                auto run = [&]()
                {
                    scale = algo::ComputeHeuristicScale(g, w, refs);
                };
                Measure(cfg, reps, run, samples);
                snprintf(config, sizeof(config), "AStar_scale,%s,%s,%s,%d,%d,%d,%d,no", graphName, queryName,
                    kPresetNames[p], g.VertexCount(), g.EdgeCount(), source, target);
                snprintf(counters, sizeof(counters), "0,0,0,0,0,%.6f,0,%.6f", 0.0, scale);
                out.Emit(cfg, config, reps, samples, counters);
            }

            algo::AlgoStats ds;
            algo::PathResult dr = algo::Dijkstra(g, source, target, w, refs, &ds);
            {
                auto run = [&]()
                {
                    algo::PathResult r = algo::Dijkstra(g, source, target, w, refs);
                    g_benchSink = g_benchSink + r.edgePath.Size();
                };
                Measure(cfg, reps, run, samples);
                snprintf(config, sizeof(config), "Dijkstra,%s,%s,%s,%d,%d,%d,%d,yes", graphName, queryName,
                    kPresetNames[p], g.VertexCount(), g.EdgeCount(), source, target);
                snprintf(counters, sizeof(counters), "%lld,%lld,%lld,%lld,%lld,%.6f,%d,%.6f", ds.verticesVisited,
                    ds.edgesRelaxed, ds.heapPushes, ds.heapPops, ds.heapPushes + ds.heapPops, dr.totalCost,
                    dr.edgePath.Size(), 0.0);
                out.Emit(cfg, config, reps, samples, counters);
            }

            algo::AlgoStats as;
            algo::PathResult ar = algo::AStar(g, source, target, w, refs, scale, &as);
            {
                auto run = [&]()
                {
                    algo::PathResult r = algo::AStar(g, source, target, w, refs, scale);
                    g_benchSink = g_benchSink + r.edgePath.Size();
                };
                Measure(cfg, reps, run, samples);
                snprintf(config, sizeof(config), "AStar,%s,%s,%s,%d,%d,%d,%d,yes", graphName, queryName,
                    kPresetNames[p], g.VertexCount(), g.EdgeCount(), source, target);
                snprintf(counters, sizeof(counters), "%lld,%lld,%lld,%lld,%lld,%.6f,%d,%.6f", as.verticesVisited,
                    as.edgesRelaxed, as.heapPushes, as.heapPops, as.heapPushes + as.heapPops, ar.totalCost,
                    ar.edgePath.Size(), scale);
                out.Emit(cfg, config, reps, samples, counters);
            }

            double diff = fabs(dr.totalCost - ar.totalCost);
            double tolerance = 1e-9 * (dr.totalCost > 1.0 ? dr.totalCost : 1.0);
            bool ok = (dr.found == ar.found) && (!dr.found || diff <= tolerance);
            CheckLogRecord("exp2", "astar_equals_dijkstra", ok,
                "%s %s %s: Dijkstra %.6f (%lld expanded) vs A* %.6f (%lld expanded)", graphName, queryName,
                kPresetNames[p], dr.totalCost, ds.verticesVisited, ar.totalCost, as.verticesVisited);
        }
    }
}

void RunExperiment2(const BenchConfig& cfg)
{
    printf("\n=== Experiment 2: Dijkstra and A* ===\n");
    ExperimentOutput out;
    if (!out.Open(cfg, "exp2_routing", "algo,graph,query,preset,V,E,source,target,includes_init",
        "expanded,relaxed,heap_pushes,heap_pops,heap_ops,path_cost,path_edges,heuristic_scale")) return;

    // Real circuits.
    const char* const circuits[3] = { "data/circuits/c1_harbor.trk", "data/circuits/c2_twinpeaks.trk",
        "data/circuits/c3_downtown.trk" };
    const char* const circuitNames[3] = { "C1_Harbor", "C2_TwinPeaks", "C3_Downtown" };
    for (int c = 0; c < 3; ++c)
    {
        io::TrackData data;
        int line = 0;
        char message[200];
        if (io::LoadTrackFile(circuits[c], &data, &line, message, sizeof(message)) != io::LoadResult::Ok)
        {
            CheckLogRecord("exp2", "load_circuit", false, "%s: %s (run from the repository root)", circuits[c], message);
            continue;
        }
        const model::Graph& g = *data.graph;
        int start = VertexOfType(g, model::VertexType::Start);
        int cp5 = CheckpointVertex(g, 5);
        int cp10 = CheckpointVertex(g, 10);
        RunQueries(cfg, out, g, circuitNames[c], "Start_to_CP5", start, cp5, 200);
        RunQueries(cfg, out, g, circuitNames[c], "CP5_to_CP10", cp5, cp10, 200);
        printf("  %s done\n", circuitNames[c]);
    }

    // Synthetic road networks.
    const int fullSizes[5] = { 1000, 5000, 10000, 50000, 100000 };
    const int quickSizes[2] = { 1000, 5000 };
    const int* sizes = cfg.quick ? quickSizes : fullSizes;
    int sizeCount = cfg.quick ? 2 : 5;
    for (int s = 0; s < sizeCount; ++s)
    {
        int side = 0;
        model::Graph* g = MakeGeometricGrid(sizes[s], cfg.seed + static_cast<unsigned long long>(sizes[s]), &side);
        int n = g->VertexCount();
        int reps = 20000 / n;
        if (reps < 1) reps = 1;

        char name[32];
        snprintf(name, sizeof(name), "grid_%d", n);
        int centre = (side / 2) * side + side / 2;
        int offset = side / 10;
        if (offset < 1) offset = 1;
        int near = (side / 2 + offset) * side + (side / 2 + offset);

        RunQueries(cfg, out, *g, name, "far_corner_to_corner", 0, n - 1, reps);
        RunQueries(cfg, out, *g, name, "near_centre_10pct", centre, near, reps * 4);
        printf("  %s done (E = %d)\n", name, g->EdgeCount());
        delete g;
    }
}
