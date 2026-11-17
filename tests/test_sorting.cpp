#include "TestFramework.h"

#include "../src/algo/Sorting.h"
#include "../src/algo/TopK.h"
#include "../src/algo/Comparators.h"
#include "../src/model/Records.h"
#include "../src/core/util/Rng.h"
#include "../src/core/ds/DynArray.h"

using namespace algo;
using namespace model;

namespace
{
    struct IntOrder
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

    enum class InputKind
    {
        Random,
        Sorted,
        Reverse,
        DuplicateHeavy,
    };

    ChampionshipEntry MakeEntry(int id, int points, int wins, int sumPos, int bestLap)
    {
        ChampionshipEntry e;
        e.racerId = id;
        e.totalPoints = points;
        e.wins = wins;
        e.sumOfFinishPositions = sumPos;
        e.bestFastestLapMs = bestLap;
        return e;
    }

    // Fills `out` with n entries (unique racer ids, so the comparator is a
    // total order) shaped like `kind`.
    void BuildEntries(InputKind kind, int n, unsigned long long seed, ChampionshipEntry* out)
    {
        Rng rng(seed);
        for (int i = 0; i < n; ++i)
        {
            if (kind == InputKind::DuplicateHeavy)
            {
                // Only a handful of distinct values per field: most
                // comparisons fall all the way through to the racer id.
                out[i] = MakeEntry(i, rng.NextInt(0, 2) * 25, rng.NextInt(0, 1), rng.NextInt(3, 4), 40000);
            }
            else
            {
                out[i] = MakeEntry(i, rng.NextInt(0, 500), rng.NextInt(0, 10),
                    rng.NextInt(1, 100), rng.NextInt(30000, 90000));
            }
        }

        if (kind == InputKind::Sorted || kind == InputKind::Reverse)
        {
            MergeSort<ChampionshipEntry, StandingOrder>(out, n);
            if (kind == InputKind::Reverse)
            {
                for (int i = 0, j = n - 1; i < j; ++i, --j)
                {
                    ChampionshipEntry t = out[i];
                    out[i] = out[j];
                    out[j] = t;
                }
            }
        }
    }

    bool IsSorted(const ChampionshipEntry* data, int n)
    {
        for (int i = 1; i < n; ++i)
        {
            if (CompareStanding(data[i - 1], data[i]) > 0) return false;
        }
        return true;
    }

    bool SameOrder(const ChampionshipEntry* a, const ChampionshipEntry* b, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            if (a[i].racerId != b[i].racerId) return false;
        }
        return true;
    }

    // Sorts three copies of the same input with the three methods and
    // checks each is sorted and all three are identical.
    bool AllThreeAgree(InputKind kind, int n, unsigned long long seed)
    {
        ChampionshipEntry* original = new ChampionshipEntry[n > 0 ? n : 1];
        ChampionshipEntry* m = new ChampionshipEntry[n > 0 ? n : 1];
        ChampionshipEntry* q = new ChampionshipEntry[n > 0 ? n : 1];
        ChampionshipEntry* h = new ChampionshipEntry[n > 0 ? n : 1];

        BuildEntries(kind, n, seed, original);
        for (int i = 0; i < n; ++i)
        {
            m[i] = original[i];
            q[i] = original[i];
            h[i] = original[i];
        }

        MergeSort<ChampionshipEntry, StandingOrder>(m, n);
        QuickSort<ChampionshipEntry, StandingOrder>(q, n);
        HeapSort<ChampionshipEntry, StandingOrder>(h, n);

        bool ok = IsSorted(m, n) && IsSorted(q, n) && IsSorted(h, n)
            && SameOrder(m, q, n) && SameOrder(m, h, n);

        delete[] original;
        delete[] m;
        delete[] q;
        delete[] h;
        return ok;
    }
}

TEST(Sorting_EmptyAndSingleton_NoCrash)
{
    int empty[1] = { 42 };
    MergeSort<int, IntOrder>(empty, 0);
    QuickSort<int, IntOrder>(empty, 0);
    HeapSort<int, IntOrder>(empty, 0);
    CHECK_EQ(empty[0], 42);

    int one[1] = { 7 };
    MergeSort<int, IntOrder>(one, 1);
    QuickSort<int, IntOrder>(one, 1);
    HeapSort<int, IntOrder>(one, 1);
    CHECK_EQ(one[0], 7);
}

TEST(Sorting_Ints_AllMethodsSortDuplicates)
{
    Rng rng(99);
    const int n = 300;
    int a[n], b[n], c[n];
    for (int i = 0; i < n; ++i)
    {
        a[i] = rng.NextInt(0, 9);   // lots of duplicates
        b[i] = a[i];
        c[i] = a[i];
    }

    MergeSort<int, IntOrder>(a, n);
    QuickSort<int, IntOrder>(b, n);
    HeapSort<int, IntOrder>(c, n);

    for (int i = 0; i < n; ++i)
    {
        CHECK_EQ(a[i], b[i]);
        CHECK_EQ(a[i], c[i]);
        if (i > 0) CHECK(a[i - 1] <= a[i]);
    }
}

TEST(Sorting_AllThreeAgree_Random)
{
    CHECK(AllThreeAgree(InputKind::Random, 1, 1));
    CHECK(AllThreeAgree(InputKind::Random, 10, 2));
    CHECK(AllThreeAgree(InputKind::Random, 257, 3));
    CHECK(AllThreeAgree(InputKind::Random, 2000, 4));
}

TEST(Sorting_AllThreeAgree_SortedAndReverse)
{
    CHECK(AllThreeAgree(InputKind::Sorted, 500, 5));
    CHECK(AllThreeAgree(InputKind::Reverse, 500, 6));
}

TEST(Sorting_AllThreeAgree_DuplicateHeavy)
{
    CHECK(AllThreeAgree(InputKind::DuplicateHeavy, 500, 7));
    CHECK(AllThreeAgree(InputKind::DuplicateHeavy, 3, 8));
}

TEST(Comparator_FullTieBreakChain)
{
    // Each pair below is tied on every criterion before the one it tests.
    ChampionshipEntry morePoints = MakeEntry(5, 50, 0, 10, 40000);
    ChampionshipEntry fewerPoints = MakeEntry(1, 40, 3, 2, 30000);
    CHECK(CompareStanding(morePoints, fewerPoints) < 0);
    CHECK(StandingTieBreakReason(morePoints, fewerPoints) == TieBreakReason::Points);

    ChampionshipEntry moreWins = MakeEntry(5, 50, 2, 10, 40000);
    ChampionshipEntry fewerWins = MakeEntry(1, 50, 1, 2, 30000);
    CHECK(CompareStanding(moreWins, fewerWins) < 0);

    ChampionshipEntry lowerSum = MakeEntry(5, 50, 1, 6, 40000);
    ChampionshipEntry higherSum = MakeEntry(1, 50, 1, 7, 30000);
    CHECK(CompareStanding(lowerSum, higherSum) < 0);

    ChampionshipEntry fasterLap = MakeEntry(5, 50, 1, 6, 30000);
    ChampionshipEntry slowerLap = MakeEntry(1, 50, 1, 6, 40000);
    CHECK(CompareStanding(fasterLap, slowerLap) < 0);

    // Identical on every criterion: only the final racer-id tie-break
    // separates them (the case the brief explicitly asks to include).
    ChampionshipEntry lowId = MakeEntry(2, 50, 1, 6, 30000);
    ChampionshipEntry highId = MakeEntry(9, 50, 1, 6, 30000);
    CHECK(CompareStanding(lowId, highId) < 0);
    CHECK(CompareStanding(highId, lowId) > 0);
    CHECK(StandingTieBreakReason(lowId, highId) == TieBreakReason::RacerId);
}

TEST(Sorting_RacerIdTieBreak_AllMethodsAgree)
{
    // Four racers tied on everything except id, given in scrambled order.
    ChampionshipEntry m[4] = {
        MakeEntry(3, 25, 1, 4, 40000), MakeEntry(0, 25, 1, 4, 40000),
        MakeEntry(2, 25, 1, 4, 40000), MakeEntry(1, 25, 1, 4, 40000),
    };
    ChampionshipEntry q[4], h[4];
    for (int i = 0; i < 4; ++i)
    {
        q[i] = m[i];
        h[i] = m[i];
    }

    MergeSort<ChampionshipEntry, StandingOrder>(m, 4);
    QuickSort<ChampionshipEntry, StandingOrder>(q, 4);
    HeapSort<ChampionshipEntry, StandingOrder>(h, 4);

    for (int i = 0; i < 4; ++i)
    {
        CHECK_EQ(m[i].racerId, i);
        CHECK_EQ(q[i].racerId, i);
        CHECK_EQ(h[i].racerId, i);
    }
}

TEST(TopK_EqualsPrefixOfFullSort_BothMethods)
{
    const int n = 200;
    ChampionshipEntry data[n];
    ChampionshipEntry sorted[n];
    BuildEntries(InputKind::Random, n, 11, data);
    for (int i = 0; i < n; ++i) sorted[i] = data[i];
    MergeSort<ChampionshipEntry, StandingOrder>(sorted, n);

    int ks[6] = { 1, 3, 10, n / 2, n - 1, n };
    for (int t = 0; t < 6; ++t)
    {
        int k = ks[t];
        ChampionshipEntry viaSizeK[n];
        ChampionshipEntry viaHeapify[n];

        int countA = TopKSizeKHeap<ChampionshipEntry, StandingOrder>(data, n, k, viaSizeK);
        int countB = TopKHeapifyExtract<ChampionshipEntry, StandingOrder>(data, n, k, viaHeapify);
        CHECK_EQ(countA, k);
        CHECK_EQ(countB, k);

        for (int i = 0; i < k; ++i)
        {
            CHECK_EQ(viaSizeK[i].racerId, sorted[i].racerId);
            CHECK_EQ(viaHeapify[i].racerId, sorted[i].racerId);
        }
    }
}

TEST(TopK_KLargerThanN_ReturnsAll)
{
    ChampionshipEntry data[3] = {
        MakeEntry(0, 10, 0, 5, 40000), MakeEntry(1, 30, 1, 2, 40000), MakeEntry(2, 20, 0, 3, 40000),
    };
    ChampionshipEntry out[3];

    int count = TopKSizeKHeap<ChampionshipEntry, StandingOrder>(data, 3, 10, out);
    CHECK_EQ(count, 3);
    CHECK_EQ(out[0].racerId, 1);
    CHECK_EQ(out[1].racerId, 2);
    CHECK_EQ(out[2].racerId, 0);

    CHECK_EQ((TopKHeapifyExtract<ChampionshipEntry, StandingOrder>(data, 3, 0, out)), 0);
}

TEST(Sorting_CountsComparisons)
{
    ChampionshipEntry data[64];
    BuildEntries(InputKind::Random, 64, 13, data);

    AlgoStats stats;
    QuickSort<ChampionshipEntry, StandingOrder>(data, 64, &stats);
    CHECK(stats.comparisons > 0);
    CHECK(stats.swaps > 0);
}
