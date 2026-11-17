// Experiment 3 -- DP and Greedy (Project.pdf section 7).
//
// Inputs: random 0/1 knapsack instances with m in {5, 10, 20, 50, 100, 200}
// items and budget B in {50, 500, 5000}. Item costs are uniform in
// 1..4B/m (so the items' total cost is about 2B and roughly half fit).
// Two benefit distributions: "uncorrelated" (benefit uniform in the same
// range as cost) and "correlated" (benefit = cost +- 10%, the harder case
// for greedy). Plus the brief's A/B/C example and our two greedy-
// suboptimal cases G1 and G2.
//
// Timed: KnapsackDp (full table + reconstruction, O(mB) time and space),
// KnapsackValueOnly (one row, O(B) space, value only), GreedyPickup
// (ratio order), ExhaustiveKnapsack (2^m subsets; m <= 20 only). Every
// call includes allocating its own table / work arrays.
//
// Untimed: DP == exhaustive on every m <= 20 configuration, on 500 random
// small instances and on the named cases; DP value == value-only DP;
// greedy quality over 50 random instances per configuration
// (exp3_greedy_quality.csv); the named cases' selected items
// (exp3_cases.csv).
#include "BenchUtil.h"
#include "../src/algo/Knapsack.h"
#include "../src/core/util/Rng.h"
#include "../src/core/ds/DynArray.h"

#include <cstdio>

namespace
{
    algo::PickupItem Item(int id, int cost, int benefit)
    {
        algo::PickupItem item;
        item.id = id;
        item.vertex = -1;
        item.cost = cost;
        item.benefit = benefit;
        return item;
    }

    void MakeInstance(int m, int budget, bool correlated, Rng& rng, DynArray<algo::PickupItem>* items)
    {
        items->Clear();
        int maxCost = (4 * budget) / m;
        if (maxCost < 1) maxCost = 1;
        for (int i = 0; i < m; ++i)
        {
            int cost = rng.NextInt(1, maxCost);
            int benefit = 0;
            if (correlated)
            {
                int spread = cost / 10;
                benefit = cost + rng.NextInt(-spread, spread);
                if (benefit < 1) benefit = 1;
            }
            else
            {
                benefit = rng.NextInt(1, maxCost);
            }
            items->Push(Item(i, cost, benefit));
        }
    }

    void FormatIds(const DynArray<int>& ids, const char* names, char* out, int cap)
    {
        int len = 0;
        out[0] = '\0';
        for (int i = 0; i < ids.Size() && len < cap - 8; ++i)
        {
            if (names != nullptr)
            {
                len += snprintf(out + len, cap - len, "%s%c", i > 0 ? " " : "", names[ids[i]]);
            }
            else
            {
                len += snprintf(out + len, cap - len, "%s%d", i > 0 ? " " : "", ids[i]);
            }
        }
    }

    void RunNamedCases(const BenchConfig& cfg)
    {
        CsvFile cases;
        if (!cases.Open(cfg, "summary", "exp3_cases.csv",
            "case,items,budget,dp_benefit,dp_cost,dp_items,greedy_benefit,greedy_cost,greedy_items,exhaustive_benefit,greedy_strictly_worse"))
        {
            return;
        }

        struct NamedCase
        {
            const char* name;
            const char* description;
            int count;
            int costs[4];
            int benefits[4];
            int budget;
        };
        const NamedCase named[3] =
        {
            { "brief_ABC", "A(6;30) B(5;24) C(5;24)", 3, { 6, 5, 5, 0 }, { 30, 24, 24, 0 }, 10 },
            { "G1_team", "costs 4;3;3 benefits 9;6;6", 3, { 4, 3, 3, 0 }, { 9, 6, 6, 0 }, 6 },
            { "G2_team", "costs 5;4;4;2 benefits 11;8;8;1", 4, { 5, 4, 4, 2 }, { 11, 8, 8, 1 }, 8 },
        };

        for (int c = 0; c < 3; ++c)
        {
            const NamedCase& nc = named[c];
            algo::PickupItem items[4];
            for (int i = 0; i < nc.count; ++i) items[i] = Item(i, nc.costs[i], nc.benefits[i]);

            algo::KnapsackResult dp = algo::KnapsackDp(items, nc.count, nc.budget);
            algo::KnapsackResult greedy = algo::GreedyPickup(items, nc.count, nc.budget);
            algo::KnapsackResult ex = algo::ExhaustiveKnapsack(items, nc.count, nc.budget);

            char dpIds[64];
            char greedyIds[64];
            const char* letters = (c == 0) ? "ABC" : nullptr;
            FormatIds(dp.selectedIds, letters, dpIds, sizeof(dpIds));
            FormatIds(greedy.selectedIds, letters, greedyIds, sizeof(greedyIds));
            bool worse = greedy.totalBenefit < dp.totalBenefit;
            cases.Row("%s,\"%s\",%d,%d,%d,%s,%d,%d,%s,%d,%s", nc.name, nc.description, nc.budget, dp.totalBenefit,
                dp.totalCost, dpIds, greedy.totalBenefit, greedy.totalCost, greedyIds, ex.totalBenefit,
                worse ? "yes" : "no");

            CheckLogRecord("exp3", "named_case_dp_equals_exhaustive", dp.totalBenefit == ex.totalBenefit,
                "%s: DP %d (%s), exhaustive %d", nc.name, dp.totalBenefit, dpIds, ex.totalBenefit);
            CheckLogRecord("exp3", "named_case_greedy_strictly_worse", worse, "%s: greedy %d (%s) < DP %d",
                nc.name, greedy.totalBenefit, greedyIds, dp.totalBenefit);
        }
    }

    void CheckRandomSmallInstances(unsigned long long seed)
    {
        Rng rng(seed ^ 0xD1CEULL);
        int matches = 0;
        const int kInstances = 500;
        DynArray<algo::PickupItem> items;
        for (int t = 0; t < kInstances; ++t)
        {
            int m = rng.NextInt(1, 12);
            int budget = rng.NextInt(0, 40);
            items.Clear();
            for (int i = 0; i < m; ++i) items.Push(Item(i, rng.NextInt(0, 15), rng.NextInt(0, 30)));
            int dp = algo::KnapsackDp(items.Data(), m, budget).totalBenefit;
            int ex = algo::ExhaustiveKnapsack(items.Data(), m, budget).totalBenefit;
            if (dp == ex) ++matches;
        }
        CheckLogRecord("exp3", "dp_equals_exhaustive_random", matches == kInstances,
            "%d/%d random instances (m 1..12, B 0..40, zero costs/benefits allowed)", matches, kInstances);
    }
}

void RunExperiment3(const BenchConfig& cfg)
{
    printf("\n=== Experiment 3: DP and Greedy ===\n");
    ExperimentOutput out;
    if (!out.Open(cfg, "exp3_knapsack", "algo,distribution,m,budget,includes_init",
        "benefit,cost,selected_items,dp_states")) return;

    CsvFile quality;
    quality.Open(cfg, "summary", "exp3_greedy_quality.csv",
        "distribution,m,budget,instances,greedy_strictly_worse,mean_greedy_over_dp,worst_greedy_over_dp");

    RunNamedCases(cfg);
    CheckRandomSmallInstances(cfg.seed);

    const int fullItems[6] = { 5, 10, 20, 50, 100, 200 };
    const int quickItems[3] = { 5, 10, 20 };
    const int* itemCounts = cfg.quick ? quickItems : fullItems;
    int itemCountN = cfg.quick ? 3 : 6;
    const int budgets[3] = { 50, 500, 5000 };
    const char* const distNames[2] = { "uncorrelated", "correlated" };

    double samples[kMaxRuns];
    char config[128];
    char counters[128];
    DynArray<algo::PickupItem> items;

    for (int d = 0; d < 2; ++d)
    {
        for (int mi = 0; mi < itemCountN; ++mi)
        {
            for (int bi = 0; bi < 3; ++bi)
            {
                int m = itemCounts[mi];
                int budget = budgets[bi];
                Rng rng(cfg.seed + static_cast<unsigned long long>(m * 100003 + budget * 7 + d));
                MakeInstance(m, budget, d == 1, rng, &items);
                const algo::PickupItem* data = items.Data();

                long long cells = static_cast<long long>(m) * budget;
                int dpReps = static_cast<int>(200000 / (cells > 0 ? cells : 1));
                if (dpReps < 1) dpReps = 1;
                if (dpReps > 2000) dpReps = 2000;

                // DP with reconstruction
                algo::AlgoStats dpStats;
                algo::KnapsackResult dp = algo::KnapsackDp(data, m, budget, &dpStats);
                {
                    auto run = [&]()
                    {
                        algo::KnapsackResult r = algo::KnapsackDp(data, m, budget);
                        g_benchSink = g_benchSink + r.totalBenefit;
                    };
                    Measure(cfg, dpReps, run, samples);
                    snprintf(config, sizeof(config), "DP_table,%s,%d,%d,yes", distNames[d], m, budget);
                    snprintf(counters, sizeof(counters), "%d,%d,%d,%lld", dp.totalBenefit, dp.totalCost,
                        dp.selectedIds.Size(), dpStats.dpStates);
                    out.Emit(cfg, config, dpReps, samples, counters);
                }

                // Value-only DP
                algo::AlgoStats voStats;
                int valueOnly = algo::KnapsackValueOnly(data, m, budget, &voStats);
                {
                    auto run = [&]()
                    {
                        g_benchSink = g_benchSink + algo::KnapsackValueOnly(data, m, budget);
                    };
                    Measure(cfg, dpReps, run, samples);
                    snprintf(config, sizeof(config), "DP_value_only,%s,%d,%d,yes", distNames[d], m, budget);
                    snprintf(counters, sizeof(counters), "%d,-1,-1,%lld", valueOnly, voStats.dpStates);
                    out.Emit(cfg, config, dpReps, samples, counters);
                }
                CheckLogRecord("exp3", "value_only_equals_dp", valueOnly == dp.totalBenefit,
                    "%s m=%d B=%d: %d vs %d", distNames[d], m, budget, valueOnly, dp.totalBenefit);

                // Greedy
                algo::KnapsackResult greedy = algo::GreedyPickup(data, m, budget);
                {
                    int reps = 20000 / m;
                    auto run = [&]()
                    {
                        algo::KnapsackResult r = algo::GreedyPickup(data, m, budget);
                        g_benchSink = g_benchSink + r.totalBenefit;
                    };
                    Measure(cfg, reps, run, samples);
                    snprintf(config, sizeof(config), "Greedy,%s,%d,%d,yes", distNames[d], m, budget);
                    snprintf(counters, sizeof(counters), "%d,%d,%d,0", greedy.totalBenefit, greedy.totalCost,
                        greedy.selectedIds.Size());
                    out.Emit(cfg, config, reps, samples, counters);
                }

                // Exhaustive (small m only)
                if (m <= algo::kExhaustiveMaxItems)
                {
                    algo::KnapsackResult ex = algo::ExhaustiveKnapsack(data, m, budget);
                    int reps = (1 << 20) >> m;
                    if (reps < 1) reps = 1;
                    if (reps > 2000) reps = 2000;
                    auto run = [&]()
                    {
                        algo::KnapsackResult r = algo::ExhaustiveKnapsack(data, m, budget);
                        g_benchSink = g_benchSink + r.totalBenefit;
                    };
                    Measure(cfg, reps, run, samples);
                    snprintf(config, sizeof(config), "Exhaustive,%s,%d,%d,yes", distNames[d], m, budget);
                    snprintf(counters, sizeof(counters), "%d,%d,%d,%lld", ex.totalBenefit, ex.totalCost,
                        ex.selectedIds.Size(), 1LL << m);
                    out.Emit(cfg, config, reps, samples, counters);
                    CheckLogRecord("exp3", "dp_equals_exhaustive", ex.totalBenefit == dp.totalBenefit,
                        "%s m=%d B=%d: DP %d, exhaustive %d", distNames[d], m, budget, dp.totalBenefit, ex.totalBenefit);
                }

                // Greedy quality over 50 fresh instances of this shape (untimed).
                int worse = 0;
                double ratioSum = 0.0;
                double worst = 1.0;
                const int kQualityInstances = 50;
                for (int t = 0; t < kQualityInstances; ++t)
                {
                    DynArray<algo::PickupItem> q;
                    MakeInstance(m, budget, d == 1, rng, &q);
                    int best = algo::KnapsackDp(q.Data(), m, budget).totalBenefit;
                    int g = algo::GreedyPickup(q.Data(), m, budget).totalBenefit;
                    double ratio = (best > 0) ? static_cast<double>(g) / best : 1.0;
                    if (g < best) ++worse;
                    if (ratio < worst) worst = ratio;
                    ratioSum += ratio;
                }
                quality.Row("%s,%d,%d,%d,%d,%.4f,%.4f", distNames[d], m, budget, kQualityInstances, worse,
                    ratioSum / kQualityInstances, worst);
            }
            printf("  %s m=%d done\n", distNames[d], itemCounts[mi]);
        }
    }
}
