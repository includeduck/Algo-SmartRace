#pragma once

#include "../core/ds/DynArray.h"
#include "AlgoStats.h"
#include "Trace.h"

namespace algo
{
    // One optional pickup the resource planner may choose (Project.pdf
    // section 5.3, Problem A). In gameplay `cost` is the extra detour time
    // the pickup adds to the planned route and `benefit` is the estimated
    // time it saves, both in TENTHS OF A SECOND -- one scalar, time, on
    // both sides, so fuel, nitro and time are never silently treated as
    // interchangeable (see sim/ResourcePlanner for how each is derived).
    // Synthetic tests use the same struct with abstract integer values.
    struct PickupItem
    {
        int id;          // deterministic tie-break key, and how results name items
        int vertex;       // pickup location in the graph (-1 for synthetic items)
        int cost;          // >= 0
        int benefit;        // >= 0
    };

    struct KnapsackResult
    {
        KnapsackResult()
            : totalBenefit(0), totalCost(0)
        {
        }

        int totalBenefit;
        int totalCost;
        DynArray<int> selectedIds;   // in increasing item-id order
    };

    // 0/1 knapsack by dynamic programming:
    //   dp[i][b] = dp[i-1][b]                                  if cost_i > b
    //   dp[i][b] = max(dp[i-1][b], dp[i-1][b-cost_i] + benefit_i)  otherwise
    // with dp[0][b] = 0 for every b. The full (m+1) x (B+1) table is kept
    // so the chosen items can be recovered by walking it back from
    // dp[m][B]. O(mB) time and O(mB) space -- pseudo-polynomial in the
    // numeric budget B, not polynomial in the input size. Items with a
    // negative cost or benefit are invalid and ignored.
    KnapsackResult KnapsackDp(const PickupItem* items, int m, int budget,
        AlgoStats* stats = nullptr, Trace* trace = nullptr);

    // Same recurrence keeping only one row: O(mB) time but O(B) space.
    // Returns the optimal total benefit only -- no table survives to
    // reconstruct which items achieved it. Used by Experiment 3 to show
    // the space trade-off.
    int KnapsackValueOnly(const PickupItem* items, int m, int budget, AlgoStats* stats = nullptr);

    // The benefit/cost-ratio greedy heuristic (section 5.3 "Greedy
    // comparison"). Order of consideration:
    //   1. zero-cost items with positive benefit: always taken (free);
    //   2. zero-benefit items: never taken (they add nothing, and a
    //      zero-cost zero-benefit item would only clutter the result);
    //   3. everything else by benefit/cost descending, ties broken by
    //      lower item id, compared by cross-multiplication (no floating
    //      point, so equal ratios really compare equal), each taken if it
    //      still fits.
    // Feasible, deterministic, and NOT guaranteed optimal.
    KnapsackResult GreedyPickup(const PickupItem* items, int m, int budget,
        AlgoStats* stats = nullptr, Trace* trace = nullptr);

    // Brute force over all 2^m subsets: the correctness oracle the DP is
    // checked against on small instances. Refuses m > kExhaustiveMaxItems
    // (returns an empty result) rather than running for hours.
    const int kExhaustiveMaxItems = 20;
    KnapsackResult ExhaustiveKnapsack(const PickupItem* items, int m, int budget, AlgoStats* stats = nullptr);
}
