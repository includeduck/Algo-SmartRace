#pragma once

#include "AlgoStats.h"

// The three ranking sorts of Act 5 (Project.pdf section 5.5), hand-written,
// all driven by the SAME comparator policy so every method produces the
// identical total order.
//
// `Cmp` is a policy type with a static method
//     static int Compare(const T& a, const T& b);
// returning < 0 if `a` ranks before `b`, > 0 if after, 0 if equal. A TOTAL
// comparator (one that never returns 0 for two distinct records, like the
// championship comparator's final racer-id tie-break) makes stability
// irrelevant: all three methods then agree element for element.
//
// Counters (when `stats` is non-null): `comparisons` counts Cmp::Compare
// calls; `swaps` counts element moves (Merge Sort) or swaps (Quick/Heap).
namespace algo
{
    namespace detail
    {
        template <typename T, typename Cmp>
        int CountedCompare(const T& a, const T& b, AlgoStats* stats)
        {
            if (stats != nullptr) ++stats->comparisons;
            return Cmp::Compare(a, b);
        }

        template <typename T>
        void CountedSwap(T* data, int i, int j, AlgoStats* stats)
        {
            T temp = data[i];
            data[i] = data[j];
            data[j] = temp;
            if (stats != nullptr) ++stats->swaps;
        }

        // ---- Merge Sort ------------------------------------------------

        template <typename T, typename Cmp>
        void MergeRange(T* data, T* buffer, int lo, int mid, int hi, AlgoStats* stats)
        {
            int i = lo;
            int j = mid;
            int k = lo;

            while (i < mid && j < hi)
            {
                // `<= 0` takes from the left run on ties: that is what makes
                // Merge Sort stable.
                if (CountedCompare<T, Cmp>(data[i], data[j], stats) <= 0)
                {
                    buffer[k] = data[i];
                    ++i;
                }
                else
                {
                    buffer[k] = data[j];
                    ++j;
                }
                ++k;
                if (stats != nullptr) ++stats->swaps;
            }
            while (i < mid)
            {
                buffer[k] = data[i];
                ++i;
                ++k;
                if (stats != nullptr) ++stats->swaps;
            }
            while (j < hi)
            {
                buffer[k] = data[j];
                ++j;
                ++k;
                if (stats != nullptr) ++stats->swaps;
            }

            for (int t = lo; t < hi; ++t) data[t] = buffer[t];
        }

        template <typename T, typename Cmp>
        void MergeSortRange(T* data, T* buffer, int lo, int hi, AlgoStats* stats)
        {
            if (hi - lo < 2) return;
            int mid = lo + (hi - lo) / 2;
            MergeSortRange<T, Cmp>(data, buffer, lo, mid, stats);
            MergeSortRange<T, Cmp>(data, buffer, mid, hi, stats);
            MergeRange<T, Cmp>(data, buffer, lo, mid, hi, stats);
        }

        // ---- Quick Sort ------------------------------------------------

        // Moves the median of data[lo], data[mid], data[hi] to data[lo] so
        // it can serve as the pivot. Median-of-three avoids the O(n^2)
        // worst case on already-sorted and reverse-sorted input.
        template <typename T, typename Cmp>
        void MedianOfThreeToFront(T* data, int lo, int hi, AlgoStats* stats)
        {
            int mid = lo + (hi - lo) / 2;
            if (CountedCompare<T, Cmp>(data[mid], data[lo], stats) < 0) CountedSwap(data, mid, lo, stats);
            if (CountedCompare<T, Cmp>(data[hi], data[lo], stats) < 0) CountedSwap(data, hi, lo, stats);
            if (CountedCompare<T, Cmp>(data[hi], data[mid], stats) < 0) CountedSwap(data, hi, mid, stats);
            // Now data[lo] <= data[mid] <= data[hi]; the median is at mid.
            CountedSwap(data, lo, mid, stats);
        }

        template <typename T, typename Cmp>
        void QuickSortRange(T* data, int lo, int hi, AlgoStats* stats)
        {
            while (lo < hi)
            {
                MedianOfThreeToFront<T, Cmp>(data, lo, hi, stats);
                T pivot = data[lo];

                // Three-way (Dutch national flag) partition:
                //   [lo, lt)   < pivot
                //   [lt, i)   == pivot
                //   (gt, hi]   > pivot
                // Equal keys are grouped and never recursed into again,
                // which keeps duplicate-heavy input at O(n log n).
                int lt = lo;
                int i = lo + 1;
                int gt = hi;
                while (i <= gt)
                {
                    int c = CountedCompare<T, Cmp>(data[i], pivot, stats);
                    if (c < 0)
                    {
                        CountedSwap(data, lt, i, stats);
                        ++lt;
                        ++i;
                    }
                    else if (c > 0)
                    {
                        CountedSwap(data, i, gt, stats);
                        --gt;
                    }
                    else
                    {
                        ++i;
                    }
                }

                // Recurse into the smaller side, loop on the larger: the
                // call stack stays O(log n) even in a bad partition.
                if (lt - lo < hi - gt)
                {
                    QuickSortRange<T, Cmp>(data, lo, lt - 1, stats);
                    lo = gt + 1;
                }
                else
                {
                    QuickSortRange<T, Cmp>(data, gt + 1, hi, stats);
                    hi = lt - 1;
                }
            }
        }

        // ---- Heap Sort -------------------------------------------------

        // Sift-down for a heap whose root is the element that ranks LAST
        // (a "max-heap" with respect to Cmp), restricted to data[0, size).
        template <typename T, typename Cmp>
        void SiftDownLast(T* data, int start, int size, AlgoStats* stats)
        {
            int root = start;
            for (;;)
            {
                int left = 2 * root + 1;
                int right = left + 1;
                int last = root;
                if (left < size && CountedCompare<T, Cmp>(data[left], data[last], stats) > 0) last = left;
                if (right < size && CountedCompare<T, Cmp>(data[right], data[last], stats) > 0) last = right;
                if (last == root) return;
                CountedSwap(data, root, last, stats);
                root = last;
            }
        }
    }

    // Top-down merge sort. O(n log n) in every case, O(n) extra space (one
    // buffer allocated once), stable.
    template <typename T, typename Cmp>
    void MergeSort(T* data, int n, AlgoStats* stats = nullptr)
    {
        if (n < 2) return;
        T* buffer = new T[n];
        detail::MergeSortRange<T, Cmp>(data, buffer, 0, n, stats);
        delete[] buffer;
    }

    // Quick sort with a median-of-three pivot and a three-way partition.
    // O(n log n) average, O(n^2) worst case (made very unlikely, not
    // impossible, by median-of-three), O(log n) stack, in place, not stable.
    template <typename T, typename Cmp>
    void QuickSort(T* data, int n, AlgoStats* stats = nullptr)
    {
        if (n < 2) return;
        detail::QuickSortRange<T, Cmp>(data, 0, n - 1, stats);
    }

    // Heap sort: bottom-up heapify in O(n), then n-1 extractions of the
    // last-ranked element to the end. O(n log n) in every case, in place,
    // not stable.
    template <typename T, typename Cmp>
    void HeapSort(T* data, int n, AlgoStats* stats = nullptr)
    {
        if (n < 2) return;
        for (int i = n / 2 - 1; i >= 0; --i) detail::SiftDownLast<T, Cmp>(data, i, n, stats);
        for (int end = n - 1; end > 0; --end)
        {
            detail::CountedSwap(data, 0, end, stats);
            detail::SiftDownLast<T, Cmp>(data, 0, end, stats);
        }
    }
}
