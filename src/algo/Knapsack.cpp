#include "Knapsack.h"
#include "Sorting.h"
#include "Pseudocode.h"

namespace algo
{
    namespace
    {
        bool IsValid(const PickupItem& item)
        {
            return item.cost >= 0 && item.benefit >= 0;
        }

        void SortSelectedIds(KnapsackResult* result)
        {
            // Small lists; an insertion sort keeps result ids in a stable,
            // comparable order without pulling a comparator policy in here.
            DynArray<int>& ids = result->selectedIds;
            for (int i = 1; i < ids.Size(); ++i)
            {
                int key = ids[i];
                int j = i - 1;
                while (j >= 0 && ids[j] > key)
                {
                    ids[j + 1] = ids[j];
                    --j;
                }
                ids[j + 1] = key;
            }
        }

        // Greedy ordering: higher benefit/cost first (cross-multiplied so
        // equal ratios compare exactly equal), then lower id.
        struct RatioOrder
        {
            static int Compare(const PickupItem& a, const PickupItem& b)
            {
                long long left = static_cast<long long>(a.benefit) * b.cost;
                long long right = static_cast<long long>(b.benefit) * a.cost;
                if (left != right) return (left > right) ? -1 : 1;
                if (a.id != b.id) return (a.id < b.id) ? -1 : 1;
                return 0;
            }

            static int Key(const PickupItem& item)
            {
                return item.id;
            }
        };
    }

    KnapsackResult KnapsackDp(const PickupItem* items, int m, int budget, AlgoStats* stats, Trace* trace)
    {
        KnapsackResult result;
        if (m <= 0 || budget < 0) return result;

        int width = budget + 1;
        int* dp = new int[(m + 1) * width];
        for (int b = 0; b < width; ++b)
        {
            dp[b] = 0;   // row 0: no items, no benefit
            if (trace != nullptr) trace->Record(TraceOp::DpCell, 0, b, 0.0, lines::knapsack::kBase);
        }

        for (int i = 1; i <= m; ++i)
        {
            const PickupItem& item = items[i - 1];
            int* row = dp + i * width;
            const int* prev = dp + (i - 1) * width;

            for (int b = 0; b < width; ++b)
            {
                int best = prev[b];   // skip item i
                if (IsValid(item) && item.cost <= b)
                {
                    int take = prev[b - item.cost] + item.benefit;
                    if (take > best) best = take;
                }
                row[b] = best;

                if (stats != nullptr) ++stats->dpStates;
                if (trace != nullptr) trace->Record(TraceOp::DpCell, i, b, static_cast<double>(best), lines::knapsack::kCell);
            }
        }

        // Reconstruction: walking back from dp[m][B], item i was taken
        // exactly when its row improved on the row above at that budget.
        int b = budget;
        for (int i = m; i >= 1; --i)
        {
            if (dp[i * width + b] != dp[(i - 1) * width + b])
            {
                const PickupItem& item = items[i - 1];
                result.selectedIds.Push(item.id);
                result.totalCost += item.cost;
                b -= item.cost;
                if (trace != nullptr) trace->Record(TraceOp::Place, i, item.id, 0.0, lines::knapsack::kTake);
            }
        }
        result.totalBenefit = dp[m * width + budget];

        delete[] dp;
        SortSelectedIds(&result);
        return result;
    }

    int KnapsackValueOnly(const PickupItem* items, int m, int budget, AlgoStats* stats)
    {
        if (m <= 0 || budget < 0) return 0;

        int* row = new int[budget + 1];
        for (int b = 0; b <= budget; ++b) row[b] = 0;

        for (int i = 0; i < m; ++i)
        {
            const PickupItem& item = items[i];
            if (!IsValid(item)) continue;

            // Iterate b DOWNWARD so row[b - cost] still holds the previous
            // item row's value: each item is then used at most once.
            for (int b = budget; b >= item.cost; --b)
            {
                int take = row[b - item.cost] + item.benefit;
                if (take > row[b]) row[b] = take;
                if (stats != nullptr) ++stats->dpStates;
            }
        }

        int value = row[budget];
        delete[] row;
        return value;
    }

    KnapsackResult GreedyPickup(const PickupItem* items, int m, int budget, AlgoStats* stats, Trace* trace)
    {
        KnapsackResult result;
        if (m <= 0 || budget < 0) return result;

        DynArray<PickupItem> ranked;
        for (int i = 0; i < m; ++i)
        {
            const PickupItem& item = items[i];
            if (!IsValid(item) || item.benefit == 0) continue;   // rule 2: zero benefit never helps

            if (item.cost == 0)
            {
                // Rule 1: free and useful -- always take, before ranking.
                result.selectedIds.Push(item.id);
                result.totalBenefit += item.benefit;
                if (trace != nullptr) trace->Record(TraceOp::Place, i, item.id, 0.0, lines::greedy::kFree);
                continue;
            }
            ranked.Push(item);
        }

        MergeSort<PickupItem, RatioOrder>(ranked.Data(), ranked.Size(), stats);

        int remaining = budget;
        for (int i = 0; i < ranked.Size(); ++i)
        {
            const PickupItem& item = ranked[i];
            if (item.cost > remaining)
            {
                if (trace != nullptr) trace->Record(TraceOp::Reject, item.id, -1, 0.0, lines::greedy::kSkip);
                continue;
            }
            remaining -= item.cost;
            result.selectedIds.Push(item.id);
            result.totalCost += item.cost;
            result.totalBenefit += item.benefit;
            if (trace != nullptr) trace->Record(TraceOp::Place, i, item.id, 0.0, lines::greedy::kTake);
        }

        SortSelectedIds(&result);
        return result;
    }

    KnapsackResult ExhaustiveKnapsack(const PickupItem* items, int m, int budget, AlgoStats* stats)
    {
        KnapsackResult result;
        if (m <= 0 || budget < 0 || m > kExhaustiveMaxItems) return result;

        unsigned int subsetCount = 1u << m;
        unsigned int bestMask = 0;
        int bestBenefit = 0;
        int bestCost = 0;

        for (unsigned int mask = 0; mask < subsetCount; ++mask)
        {
            int cost = 0;
            int benefit = 0;
            bool valid = true;
            for (int i = 0; i < m && valid; ++i)
            {
                if ((mask & (1u << i)) == 0) continue;
                if (!IsValid(items[i]))
                {
                    valid = false;
                    break;
                }
                cost += items[i].cost;
                benefit += items[i].benefit;
            }
            if (stats != nullptr) ++stats->nodesExplored;
            if (!valid || cost > budget) continue;

            // Strictly better only: among equal-benefit subsets the first
            // (lowest mask) wins, so the oracle is deterministic too.
            if (benefit > bestBenefit)
            {
                bestBenefit = benefit;
                bestCost = cost;
                bestMask = mask;
            }
        }

        for (int i = 0; i < m; ++i)
        {
            if ((bestMask & (1u << i)) != 0) result.selectedIds.Push(items[i].id);
        }
        result.totalBenefit = bestBenefit;
        result.totalCost = bestCost;
        SortSelectedIds(&result);
        return result;
    }
}
