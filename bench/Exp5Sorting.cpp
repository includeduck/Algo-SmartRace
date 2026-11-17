// Experiment 5 -- Sorting and Top-k (Project.pdf section 7).
//
// Records: model::ChampionshipEntry ranked by algo::StandingOrder, the one
// championship comparator (points desc, wins desc, sum of finish positions
// asc, best lap asc, racer id asc) every sort and top-k in the game uses.
// n in {10, 100, 1k, 10k, 100k}; inputs:
//   random     points 0..78, wins 0..3, position sum 3..30, best lap 50..90 s
//   sorted     the random records already in comparator order
//   reverse    the sorted records reversed
//   duplicate  only 24 distinct (points, wins, sum, lap) keys, so long runs
//              of records differ only by racer id (the final tie-breaker)
// Racer ids are a random permutation of 0..n-1 in every input.
//
// Timed: MergeSort, QuickSort (median of 3 seeded samples, 3-way partition),
// HeapSort, each on a fresh copy of the input (the copy is made before the
// timer starts). Small n are batched: `reps` copies are prepared, then all
// of them are sorted inside one timed region.
// Top-k on the random input, n in {1k, 10k, 100k}, k in {1, 10, n/2, n-1}:
// size-k heap (O(n log k)), heapify + k extractions (O(n + k log n)), and a
// full Merge Sort for comparison.
//
// Correctness (untimed): the three sorts produce identical rankings, each
// ranking is strictly increasing under the comparator, the duplicate input
// really needs the racer-id tie-breaker, and both top-k methods equal the
// first k entries of the full sort.
#include "BenchUtil.h"
#include "../src/algo/Sorting.h"
#include "../src/algo/TopK.h"
#include "../src/algo/Comparators.h"
#include "../src/core/util/Rng.h"
#include "../src/core/ds/DynArray.h"

#include <cstdio>

namespace
{
    typedef model::ChampionshipEntry Entry;
    typedef algo::StandingOrder Order;

    enum InputKind
    {
        kRandom,
        kSorted,
        kReverse,
        kDuplicate,
        kInputCount,
    };
    const char* const kInputNames[kInputCount] = { "random", "sorted", "reverse", "duplicate" };
    const char* const kSortNames[3] = { "MergeSort", "QuickSort", "HeapSort" };

    void MakeInput(InputKind kind, int n, unsigned long long seed, DynArray<Entry>* out)
    {
        Rng rng(seed);
        out->Clear();

        // Racer ids: a random permutation (Fisher-Yates).
        DynArray<int> ids;
        for (int i = 0; i < n; ++i) ids.Push(i);
        for (int i = n - 1; i > 0; --i)
        {
            int j = rng.NextInt(0, i);
            int t = ids[i];
            ids[i] = ids[j];
            ids[j] = t;
        }

        for (int i = 0; i < n; ++i)
        {
            Entry e;
            e.racerId = ids[i];
            if (kind == kDuplicate)
            {
                e.totalPoints = 25 * rng.NextInt(0, 2);
                e.wins = rng.NextInt(0, 1);
                e.sumOfFinishPositions = 6 + rng.NextInt(0, 1);
                e.bestFastestLapMs = 60000 + 1000 * rng.NextInt(0, 1);
            }
            else
            {
                e.totalPoints = rng.NextInt(0, 78);
                e.wins = rng.NextInt(0, 3);
                e.sumOfFinishPositions = rng.NextInt(3, 30);
                e.bestFastestLapMs = rng.NextInt(50000, 90000);
            }
            out->Push(e);
        }

        if (kind == kSorted || kind == kReverse)
        {
            algo::MergeSort<Entry, Order>(out->Data(), n);
            if (kind == kReverse)
            {
                for (int i = 0, j = n - 1; i < j; ++i, --j)
                {
                    Entry t = (*out)[i];
                    (*out)[i] = (*out)[j];
                    (*out)[j] = t;
                }
            }
        }
    }

    void SortWith(int method, Entry* data, int n, algo::AlgoStats* stats)
    {
        if (method == 0) algo::MergeSort<Entry, Order>(data, n, stats);
        if (method == 1) algo::QuickSort<Entry, Order>(data, n, stats);
        if (method == 2) algo::HeapSort<Entry, Order>(data, n, stats);
    }

    // Times `method` on fresh copies of `input`; samples are per sort.
    void TimeSort(const BenchConfig& cfg, int method, const DynArray<Entry>& input, int reps, Entry* pool, double* samples)
    {
        int n = input.Size();
        for (int r = -cfg.warmup; r < cfg.runs; ++r)
        {
            for (int k = 0; k < reps; ++k)
            {
                for (int i = 0; i < n; ++i) pool[k * n + i] = input[i];
            }
            double t0 = Timer::NowMicros();
            for (int k = 0; k < reps; ++k) SortWith(method, pool + k * n, n, nullptr);
            double t1 = Timer::NowMicros();
            if (r >= 0) samples[r] = (t1 - t0) / reps;
            g_benchSink = g_benchSink + pool[0].racerId;
        }
    }

    bool SameRanking(const Entry* a, const Entry* b, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            if (a[i].racerId != b[i].racerId) return false;
        }
        return true;
    }

    bool StrictlyOrdered(const Entry* a, int n)
    {
        for (int i = 1; i < n; ++i)
        {
            if (Order::Compare(a[i - 1], a[i]) >= 0) return false;
        }
        return true;
    }

    int RacerIdTieBreaks(const Entry* a, int n)
    {
        int count = 0;
        for (int i = 1; i < n; ++i)
        {
            if (algo::StandingTieBreakReason(a[i - 1], a[i]) == algo::TieBreakReason::RacerId) ++count;
        }
        return count;
    }

    void RunSorts(const BenchConfig& cfg)
    {
        ExperimentOutput out;
        if (!out.Open(cfg, "exp5_sorting", "algo,input,n,includes_init", "comparisons,swaps_or_moves")) return;

        const int fullSizes[5] = { 10, 100, 1000, 10000, 100000 };
        const int quickSizes[3] = { 10, 100, 1000 };
        const int* sizes = cfg.quick ? quickSizes : fullSizes;
        int sizeCount = cfg.quick ? 3 : 5;

        double samples[kMaxRuns];
        char config[96];
        char counters[64];
        DynArray<Entry> input;

        for (int s = 0; s < sizeCount; ++s)
        {
            int n = sizes[s];
            int reps = 100000 / n;
            if (reps < 1) reps = 1;
            if (reps > 1000) reps = 1000;
            Entry* pool = new Entry[reps * n];

            for (int kind = 0; kind < kInputCount; ++kind)
            {
                MakeInput(static_cast<InputKind>(kind), n, cfg.seed + static_cast<unsigned long long>(n) * 7 + kind, &input);

                Entry* results[3];
                for (int m = 0; m < 3; ++m)
                {
                    results[m] = new Entry[n];
                    for (int i = 0; i < n; ++i) results[m][i] = input[i];
                    algo::AlgoStats stats;
                    SortWith(m, results[m], n, &stats);

                    TimeSort(cfg, m, input, reps, pool, samples);
                    snprintf(config, sizeof(config), "%s,%s,%d,no", kSortNames[m], kInputNames[kind], n);
                    snprintf(counters, sizeof(counters), "%lld,%lld", stats.comparisons, stats.swaps);
                    out.Emit(cfg, config, reps, samples, counters);
                }

                bool agree = SameRanking(results[0], results[1], n) && SameRanking(results[0], results[2], n);
                bool ordered = StrictlyOrdered(results[0], n);
                CheckLogRecord("exp5", "three_sorts_agree", agree && ordered, "%s n=%d: Merge = Quick = Heap, strictly ordered",
                    kInputNames[kind], n);
                if (kind == kDuplicate && n >= 100)
                {
                    int ties = RacerIdTieBreaks(results[0], n);
                    CheckLogRecord("exp5", "racer_id_tiebreak_exercised", ties > 0,
                        "duplicate n=%d: %d adjacent pairs ordered only by racer id", n, ties);
                }
                for (int m = 0; m < 3; ++m) delete[] results[m];
            }
            delete[] pool;
            printf("  sorts n=%d done\n", n);
        }
    }

    void RunTopK(const BenchConfig& cfg)
    {
        ExperimentOutput out;
        if (!out.Open(cfg, "exp5_topk", "algo,n,k,includes_init", "comparisons")) return;

        const int fullSizes[3] = { 1000, 10000, 100000 };
        const int quickSizes[2] = { 1000, 10000 };
        const int* sizes = cfg.quick ? quickSizes : fullSizes;
        int sizeCount = cfg.quick ? 2 : 3;

        double samples[kMaxRuns];
        char config[96];
        char counters[64];
        DynArray<Entry> input;

        for (int s = 0; s < sizeCount; ++s)
        {
            int n = sizes[s];
            // Same seed as the sorting section's random input of this size.
            MakeInput(kRandom, n, cfg.seed + static_cast<unsigned long long>(n) * 7 + kRandom, &input);
            int reps = 100000 / n;
            if (reps < 1) reps = 1;

            Entry* sorted = new Entry[n];
            for (int i = 0; i < n; ++i) sorted[i] = input[i];
            algo::AlgoStats fullStats;
            algo::MergeSort<Entry, Order>(sorted, n, &fullStats);

            Entry* pool = new Entry[reps * n];
            TimeSort(cfg, 0, input, reps, pool, samples);
            delete[] pool;
            snprintf(config, sizeof(config), "MergeSort_full,%d,%d,no", n, n);
            snprintf(counters, sizeof(counters), "%lld", fullStats.comparisons);
            out.Emit(cfg, config, reps, samples, counters);

            Entry* top = new Entry[n];
            const int ks[4] = { 1, 10, n / 2, n - 1 };
            for (int ki = 0; ki < 4; ++ki)
            {
                int k = ks[ki];
                for (int method = 0; method < 2; ++method)
                {
                    algo::AlgoStats stats;
                    int written = (method == 0)
                        ? algo::TopKSizeKHeap<Entry, Order>(input.Data(), n, k, top, &stats)
                        : algo::TopKHeapifyExtract<Entry, Order>(input.Data(), n, k, top, &stats);
                    bool prefix = (written == k) && SameRanking(top, sorted, k);
                    CheckLogRecord("exp5", "topk_equals_sort_prefix", prefix, "%s n=%d k=%d",
                        method == 0 ? "size-k heap" : "heapify+extract", n, k);

                    const Entry* data = input.Data();
                    auto run = [&]()
                    {
                        int w = (method == 0)
                            ? algo::TopKSizeKHeap<Entry, Order>(data, n, k, top)
                            : algo::TopKHeapifyExtract<Entry, Order>(data, n, k, top);
                        g_benchSink = g_benchSink + w;
                    };
                    Measure(cfg, reps, run, samples);
                    snprintf(config, sizeof(config), "%s,%d,%d,yes", method == 0 ? "TopK_size_k_heap" : "TopK_heapify_extract", n, k);
                    snprintf(counters, sizeof(counters), "%lld", stats.comparisons);
                    out.Emit(cfg, config, reps, samples, counters);
                }
            }
            delete[] top;
            delete[] sorted;
            printf("  top-k n=%d done\n", n);
        }
    }
}

void RunExperiment5(const BenchConfig& cfg)
{
    printf("\n=== Experiment 5: Sorting and top-k ===\n");
    RunSorts(cfg);
    RunTopK(cfg);
}
