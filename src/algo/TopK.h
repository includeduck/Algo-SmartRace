#pragma once

#include "AlgoStats.h"
#include "../core/ds/MinHeap.h"

// Top-k retrieval (Project.pdf section 5.5 and Challenge 3), driven by the
// same comparator policy as the sorts in Sorting.h, so the first k entries
// of any full sort and the output of either function below are identical.
// Both reuse the project's one binary-heap implementation (MinHeap) with
// an adapter policy, rather than a second heap written just for this.
namespace algo
{
    namespace detail
    {
        // MinHeap ordering where the heap top is the element that ranks
        // LAST under Cmp -- the "worst of the current top k".
        template <typename T, typename Cmp>
        struct WorstOnTop
        {
            static bool Less(const T& a, const T& b)
            {
                return Cmp::Compare(a, b) > 0;
            }
        };

        // MinHeap ordering where the heap top is the element that ranks
        // FIRST under Cmp.
        template <typename T, typename Cmp>
        struct BestOnTop
        {
            static bool Less(const T& a, const T& b)
            {
                return Cmp::Compare(a, b) < 0;
            }
        };
    }

    // Size-k heap: keep the best k seen so far in a heap whose top is the
    // worst of them; each new element only enters if it beats that top.
    // O(n log k) time, O(k) extra space. Writes min(k, n) entries to `out`,
    // best first. Returns how many were written.
    template <typename T, typename Cmp>
    int TopKSizeKHeap(const T* data, int n, int k, T* out, AlgoStats* stats = nullptr)
    {
        if (k <= 0 || n <= 0) return 0;

        MinHeap<T, detail::WorstOnTop<T, Cmp>> heap;
        long long extraComparisons = 0;
        long long heapOps = 0;

        for (int i = 0; i < n; ++i)
        {
            if (heap.Size() < k)
            {
                heap.Push(data[i]);
                ++heapOps;
                continue;
            }

            ++extraComparisons;
            if (Cmp::Compare(data[i], heap.Top()) < 0)
            {
                heap.Pop();
                heap.Push(data[i]);
                heapOps += 2;
            }
        }

        // Popping yields worst-first, so fill `out` from the back.
        int count = heap.Size();
        for (int i = count - 1; i >= 0; --i)
        {
            out[i] = heap.Pop();
            ++heapOps;
        }

        if (stats != nullptr)
        {
            stats->comparisons += heap.comparisons + extraComparisons;
            stats->swaps += heap.swaps;
            stats->heapPushes += heapOps;
        }
        return count;
    }

    // Heapify-then-extract: build one heap of all n elements in O(n)
    // (bottom-up heapify), then pop the best k. O(n + k log n) time, O(n)
    // extra space. Better than the size-k heap when k is close to n.
    template <typename T, typename Cmp>
    int TopKHeapifyExtract(const T* data, int n, int k, T* out, AlgoStats* stats = nullptr)
    {
        if (k <= 0 || n <= 0) return 0;
        if (k > n) k = n;

        MinHeap<T, detail::BestOnTop<T, Cmp>> heap;
        heap.Heapify(data, n);
        for (int i = 0; i < k; ++i) out[i] = heap.Pop();

        if (stats != nullptr)
        {
            stats->comparisons += heap.comparisons;
            stats->swaps += heap.swaps;
            stats->heapPops += k;
        }
        return k;
    }
}
