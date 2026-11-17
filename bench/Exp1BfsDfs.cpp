// Experiment 1 -- BFS and DFS (Project.pdf section 7).
//
// Inputs: random connected undirected graphs, V in {1k, 5k, 10k, 50k, 100k},
// average degree 3 (sparse) and 12 (denser). Timed: BFS full reachability
// from vertex 0, DFS traversal from vertex 0, DFS connected components and
// DFS cycle detection. Each timed call includes allocating its own result
// arrays (visited/parent/distance), which is part of the algorithm's O(V)
// work.
//
// Correctness (untimed):
//   * BFS hop distances equal those of a brute-force fixed-point relaxation,
//     and satisfy the BFS certificate (|d(u) - d(v)| <= 1 on every edge,
//     every reached vertex's parent is one hop closer).
//   * DFS traversal reaches every vertex of a connected graph.
//   * DFS components equal the union-find partition on a 4-component graph.
//   * DFS finds a cycle in every test graph and none in a forest.
#include "BenchUtil.h"
#include "GraphGen.h"
#include "../src/algo/Bfs.h"
#include "../src/algo/Dfs.h"
#include "../src/core/ds/UnionFind.h"
#include "../src/core/ds/DynArray.h"

#include <cstdio>

namespace
{
    // Brute force: start with d(s) = 0, others infinity, and relax every
    // edge (unit weight) until nothing changes. O(diameter * E).
    void BruteForceHops(const model::Graph& g, int source, DynArray<int>* dist)
    {
        int n = g.VertexCount();
        dist->Clear();
        for (int v = 0; v < n; ++v) dist->Push(-1);
        (*dist)[source] = 0;

        bool changed = true;
        while (changed)
        {
            changed = false;
            for (int e = 0; e < g.EdgeCount(); ++e)
            {
                int a = g.GetEdge(e).from;
                int b = g.GetEdge(e).to;
                if ((*dist)[a] >= 0 && ((*dist)[b] < 0 || (*dist)[a] + 1 < (*dist)[b]))
                {
                    (*dist)[b] = (*dist)[a] + 1;
                    changed = true;
                }
                if ((*dist)[b] >= 0 && ((*dist)[a] < 0 || (*dist)[b] + 1 < (*dist)[a]))
                {
                    (*dist)[a] = (*dist)[b] + 1;
                    changed = true;
                }
            }
        }
    }

    bool BfsCertificateHolds(const model::Graph& g, int source, const algo::BfsResult& r)
    {
        if (r.distEdges[source] != 0) return false;
        for (int e = 0; e < g.EdgeCount(); ++e)
        {
            int a = r.distEdges[g.GetEdge(e).from];
            int b = r.distEdges[g.GetEdge(e).to];
            if ((a < 0) != (b < 0)) return false;
            if (a >= 0 && (a - b > 1 || b - a > 1)) return false;
        }
        for (int v = 0; v < g.VertexCount(); ++v)
        {
            if (v == source || r.distEdges[v] < 0) continue;
            int p = r.parent[v];
            if (p < 0 || r.distEdges[p] != r.distEdges[v] - 1) return false;
        }
        return true;
    }

    void CheckGraph(const model::Graph& g, const char* label)
    {
        algo::BfsResult bfs = algo::Bfs(g, 0, -1);
        DynArray<int> brute;
        BruteForceHops(g, 0, &brute);
        bool same = true;
        for (int v = 0; v < g.VertexCount(); ++v)
        {
            if (brute[v] != bfs.distEdges[v]) same = false;
        }
        CheckLogRecord("exp1", "bfs_matches_bruteforce", same, "%s: BFS hop distances vs fixed-point relaxation", label);
        CheckLogRecord("exp1", "bfs_certificate", BfsCertificateHolds(g, 0, bfs), "%s", label);

        algo::DfsTraversalResult dfs = algo::DfsTraversal(g, 0);
        CheckLogRecord("exp1", "dfs_reaches_all", dfs.order.Size() == g.VertexCount(), "%s: visited %d of %d",
            label, dfs.order.Size(), g.VertexCount());

        algo::DfsCycleResult cyc = algo::DfsCycles(g);
        CheckLogRecord("exp1", "dfs_finds_cycle", cyc.hasCycle, "%s: E = %d > V - 1", label, g.EdgeCount());
    }

    void CheckComponentsAgainstUnionFind(unsigned long long seed)
    {
        const int kComponents = 4;
        model::Graph* g = MakeRandomGraph(2000, 3.0, kComponents, seed);
        algo::DfsComponentsResult comps = algo::DfsComponents(*g);

        UnionFind uf;
        uf.Init(g->VertexCount());
        for (int e = 0; e < g->EdgeCount(); ++e) uf.Union(g->GetEdge(e).from, g->GetEdge(e).to);

        // Same partition: equal counts, and every DFS component maps to a
        // single union-find root.
        DynArray<int> rootOfComponent;
        for (int c = 0; c < comps.componentCount; ++c) rootOfComponent.Push(-1);
        bool consistent = (comps.componentCount == uf.ComponentCount());
        for (int v = 0; v < g->VertexCount() && consistent; ++v)
        {
            int c = comps.componentId[v];
            int root = uf.Find(v);
            if (rootOfComponent[c] < 0) rootOfComponent[c] = root;
            if (rootOfComponent[c] != root) consistent = false;
        }
        CheckLogRecord("exp1", "dfs_components_match_unionfind", consistent,
            "V = 2000, built with %d components: DFS %d, union-find %d", kComponents, comps.componentCount,
            uf.ComponentCount());

        model::Graph* forest = MakeRandomGraph(2000, 0.0, kComponents, seed + 1);
        algo::DfsCycleResult cyc = algo::DfsCycles(*forest);
        CheckLogRecord("exp1", "dfs_no_cycle_in_forest", !cyc.hasCycle, "forest V = 2000, E = %d", forest->EdgeCount());

        delete g;
        delete forest;
    }

    void CheckBriefExample()
    {
        // S-T direct (length 10) vs S-A-B-T (three roads of length 2).
        model::Graph g;
        int s = g.AddVertex(0, 0, model::VertexType::Start);
        int a = g.AddVertex(1, 1, model::VertexType::Intersection);
        int b = g.AddVertex(2, 1, model::VertexType::Intersection);
        int t = g.AddVertex(3, 0, model::VertexType::Finish);
        g.AddEdge(s, t, 10.0, 1.0, 1.0, 1, false, false);
        g.AddEdge(s, a, 2.0, 1.0, 1.0, 1, false, false);
        g.AddEdge(a, b, 2.0, 1.0, 1.0, 1, false, false);
        g.AddEdge(b, t, 2.0, 1.0, 1.0, 1, false, false);
        algo::BfsResult r = algo::Bfs(g, s, t);
        CheckLogRecord("exp1", "bfs_brief_example", r.found && r.distEdges[t] == 1,
            "S-T: BFS picks the 1-road path (distance 10) over the 3-road path (distance 6)");
    }
}

void RunExperiment1(const BenchConfig& cfg)
{
    printf("\n=== Experiment 1: BFS and DFS ===\n");
    ExperimentOutput out;
    if (!out.Open(cfg, "exp1_bfs_dfs", "algo,density,V,E,includes_init",
        "visited,edges_examined")) return;

    const int fullSizes[5] = { 1000, 5000, 10000, 50000, 100000 };
    const int quickSizes[2] = { 1000, 5000 };
    const int* sizes = cfg.quick ? quickSizes : fullSizes;
    int sizeCount = cfg.quick ? 2 : 5;
    const double degrees[2] = { 3.0, 12.0 };
    const char* densityNames[2] = { "sparse_deg3", "dense_deg12" };

    double samples[kMaxRuns];
    char config[160];
    char counters[96];

    CheckBriefExample();
    CheckComponentsAgainstUnionFind(cfg.seed);

    for (int d = 0; d < 2; ++d)
    {
        for (int s = 0; s < sizeCount; ++s)
        {
            int n = sizes[s];
            model::Graph* g = MakeRandomGraph(n, degrees[d], 1, cfg.seed + static_cast<unsigned long long>(n) + d);
            int e = g->EdgeCount();
            int reps = 20000 / n;
            if (reps < 1) reps = 1;

            char label[64];
            snprintf(label, sizeof(label), "%s V=%d", densityNames[d], n);
            CheckGraph(*g, label);

            // BFS
            {
                algo::AlgoStats stats;
                auto run = [&]()
                {
                    algo::BfsResult r = algo::Bfs(*g, 0, -1);
                    g_benchSink = g_benchSink + r.distEdges[n - 1];
                };
                Measure(cfg, reps, run, samples);
                algo::Bfs(*g, 0, -1, &stats);
                snprintf(config, sizeof(config), "BFS,%s,%d,%d,yes", densityNames[d], n, e);
                snprintf(counters, sizeof(counters), "%lld,%lld", stats.verticesVisited, stats.edgesExamined);
                out.Emit(cfg, config, reps, samples, counters);
            }
            // DFS traversal
            {
                algo::AlgoStats stats;
                auto run = [&]()
                {
                    algo::DfsTraversalResult r = algo::DfsTraversal(*g, 0);
                    g_benchSink = g_benchSink + r.order.Size();
                };
                Measure(cfg, reps, run, samples);
                algo::DfsTraversal(*g, 0, &stats);
                snprintf(config, sizeof(config), "DFS_traversal,%s,%d,%d,yes", densityNames[d], n, e);
                snprintf(counters, sizeof(counters), "%lld,%lld", stats.verticesVisited, stats.edgesExamined);
                out.Emit(cfg, config, reps, samples, counters);
            }
            // DFS components
            {
                algo::AlgoStats stats;
                auto run = [&]()
                {
                    algo::DfsComponentsResult r = algo::DfsComponents(*g);
                    g_benchSink = g_benchSink + r.componentCount;
                };
                Measure(cfg, reps, run, samples);
                algo::DfsComponents(*g, &stats);
                snprintf(config, sizeof(config), "DFS_components,%s,%d,%d,yes", densityNames[d], n, e);
                snprintf(counters, sizeof(counters), "%lld,%lld", stats.verticesVisited, stats.edgesExamined);
                out.Emit(cfg, config, reps, samples, counters);
            }
            // DFS cycle detection
            {
                algo::AlgoStats stats;
                auto run = [&]()
                {
                    algo::DfsCycleResult r = algo::DfsCycles(*g);
                    g_benchSink = g_benchSink + (r.hasCycle ? 1 : 0);
                };
                Measure(cfg, reps, run, samples);
                algo::DfsCycles(*g, &stats);
                snprintf(config, sizeof(config), "DFS_cycles,%s,%d,%d,yes", densityNames[d], n, e);
                snprintf(counters, sizeof(counters), "%lld,%lld", stats.verticesVisited, stats.edgesExamined);
                out.Emit(cfg, config, reps, samples, counters);
            }

            printf("  %s done (E = %d)\n", label, e);
            delete g;
        }
    }
}
