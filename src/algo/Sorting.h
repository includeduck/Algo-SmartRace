#pragma once

#include "AlgoStats.h"
#include "Trace.h"
#include "Pseudocode.h"

// The three ranking sorts of Act 5 (Project.pdf section 5.5), hand-written,
// all driven by the SAME comparator policy so every method produces the
// identical total order.
//
// `Cmp` is a policy type with two static methods:
//     static int Compare(const T& a, const T& b);   // < 0: a ranks first
//     static int Key(const T& item);                // the item's display id
// Compare must be a TOTAL order for the three methods to agree element for
// element (the championship comparator's final racer-id tie-break makes it
// one). Key is what the Algorithm Lab shows for an element (a racer id, an
// item id, a value) when Merge Sort writes it into a new slot.
//
// Counters (when `stats` is non-null): `comparisons` counts Cmp::Compare
// calls; `swaps` counts element moves (Merge Sort) or swaps (Quick/Heap).
// Trace (when `trace` is non-null, the Lab only): Compare(i, j) by array
// index (j = -1 when comparing against Quick Sort's pivot copy), Swap(i, j),
// and Place(i, key) for each Merge Sort write-back.
namespace algo
{
    namespace detail
    {
        template <typename T, typename Cmp>
        int CompareAt(const T& a, const T& b, int ia, int ib, AlgoStats* stats, Trace* trace, int line)
        {
            if (stats != nullptr) ++stats->comparisons;
            if (trace != nullptr) trace->Record(TraceOp::Compare, ia, ib, 0.0, line);
            return Cmp::Compare(a, b);
        }

        template <typename T>
        void SwapAt(T* data, int i, int j, AlgoStats* stats, Trace* trace, int line)
        {
            T temp = data[i];
            data[i] = data[j];
            data[j] = temp;
            if (stats != nullptr) ++stats->swaps;
            if (trace != nullptr) trace->Record(TraceOp::Swap, i, j, 0.0, line);
        }

        // ---- Merge Sort ------------------------------------------------

        template <typename T, typename Cmp>
        void MergeRange(T* data, T* buffer, int lo, int mid, int hi, AlgoStats* stats, Trace* trace)
        {
            int i = lo;
            int j = mid;
            int k = lo;

            while (i < mid && j < hi)
            {
                // `<= 0` takes from the left run on ties: that is what makes
                // Merge Sort stable.
                if (CompareAt<T, Cmp>(data[i], data[j], i, j, stats, trace, lines::merge::kCompare) <= 0)
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

            for (int t = lo; t < hi; ++t)
            {
                data[t] = buffer[t];
                if (trace != nullptr) trace->Record(TraceOp::Place, t, Cmp::Key(data[t]), 0.0, lines::merge::kPlace);
            }
        }

        template <typename T, typename Cmp>
        void MergeSortRange(T* data, T* buffer, int lo, int hi, AlgoStats* stats, Trace* trace)
        {
            if (hi - lo < 2) return;
            int mid = lo + (hi - lo) / 2;
            MergeSortRange<T, Cmp>(data, buffer, lo, mid, stats, trace);
            MergeSortRange<T, Cmp>(data, buffer, mid, hi, stats, trace);
            MergeRange<T, Cmp>(data, buffer, lo, mid, hi, stats, trace);
        }

        // ---- Quick Sort ------------------------------------------------

        // Moves the median of data[lo], data[mid], data[hi] to data[lo] so
        // it can serve as the pivot. Median-of-three avoids the O(n^2)
        // worst case on already-sorted and reverse-sorted input.
        template <typename T, typename Cmp>
        void MedianOfThreeToFront(T* data, int lo, int hi, AlgoStats* stats, Trace* trace)
        {
            const int line = lines::quick::kMedian;
            int mid = lo + (hi - lo) / 2;
            if (CompareAt<T, Cmp>(data[mid], data[lo], mid, lo, stats, trace, line) < 0) SwapAt(data, mid, lo, stats, trace, line);
            if (CompareAt<T, Cmp>(data[hi], data[lo], hi, lo, stats, trace, line) < 0) SwapAt(data, hi, lo, stats, trace, line);
            if (CompareAt<T, Cmp>(data[hi], data[mid], hi, mid, stats, trace, line) < 0) SwapAt(data, hi, mid, stats, trace, line);
            // Now data[lo] <= data[mid] <= data[hi]; the median is at mid.
            SwapAt(data, lo, mid, stats, trace, line);
        }

        template <typename T, typename Cmp>
        void QuickSortRange(T* data, int lo, int hi, AlgoStats* stats, Trace* trace)
        {
            while (lo < hi)
            {
                MedianOfThreeToFront<T, Cmp>(data, lo, hi, stats, trace);
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
                    int c = CompareAt<T, Cmp>(data[i], pivot, i, -1, stats, trace, lines::quick::kCompare);
                    if (c < 0)
                    {
                        SwapAt(data, lt, i, stats, trace, lines::quick::kSwap);
                        ++lt;
                        ++i;
                    }
                    else if (c > 0)
                    {
                        SwapAt(data, i, gt, stats, trace, lines::quick::kSwap);
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
                    QuickSortRange<T, Cmp>(data, lo, lt - 1, stats, trace);
                    lo = gt + 1;
                }
                else
                {
                    QuickSortRange<T, Cmp>(data, gt + 1, hi, stats, trace);
                    hi = lt - 1;
                }
            }
        }

        // ---- Heap Sort -------------------------------------------------

        // Sift-down for a heap whose root is the element that ranks LAST
        // (a "max-heap" with respect to Cmp), restricted to data[0, size).
        template <typename T, typename Cmp>
        void SiftDownLast(T* data, int start, int size, AlgoStats* stats, Trace* trace)
        {
            const int line = lines::heap::kSift;
            int root = start;
            for (;;)
            {
                int left = 2 * root + 1;
                int right = left + 1;
                int last = root;
                if (left < size && CompareAt<T, Cmp>(data[left], data[last], left, last, stats, trace, line) > 0) last = left;
                if (right < size && CompareAt<T, Cmp>(data[right], data[last], right, last, stats, trace, line) > 0) last = right;
                if (last == root) return;
                SwapAt(data, root, last, stats, trace, line);
                root = last;
            }
        }
    }

    // Top-down merge sort. O(n log n) in every case, O(n) extra space (one
    // buffer allocated once), stable.
    template <typename T, typename Cmp>
    void MergeSort(T* data, int n, AlgoStats* stats = nullptr, Trace* trace = nullptr)
    {
        if (n < 2) return;
        T* buffer = new T[n];
        detail::MergeSortRange<T, Cmp>(data, buffer, 0, n, stats, trace);
        delete[] buffer;
    }

    // Quick sort with a median-of-three pivot and a three-way partition.
    // O(n log n) average, O(n^2) worst case (made very unlikely, not
    // impossible, by median-of-three), O(log n) stack, in place, not stable.
    template <typename T, typename Cmp>
    void QuickSort(T* data, int n, AlgoStats* stats = nullptr, Trace* trace = nullptr)
    {
        if (n < 2) return;
        detail::QuickSortRange<T, Cmp>(data, 0, n - 1, stats, trace);
    }

    // Heap sort: bottom-up heapify in O(n), then n-1 extractions of the
    // last-ranked element to the end. O(n log n) in every case, in place,
    // not stable.
    template <typename T, typename Cmp>
    void HeapSort(T* data, int n, AlgoStats* stats = nullptr, Trace* trace = nullptr)
    {
        if (n < 2) return;
        for (int i = n / 2 - 1; i >= 0; --i) detail::SiftDownLast<T, Cmp>(data, i, n, stats, trace);
        for (int end = n - 1; end > 0; --end)
        {
            detail::SwapAt(data, 0, end, stats, trace, lines::heap::kExtract);
            detail::SiftDownLast<T, Cmp>(data, 0, end, stats, trace);
        }
    }
}
