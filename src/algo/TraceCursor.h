#pragma once

#include "Trace.h"
#include "../core/ds/DynArray.h"

namespace algo
{
    // What a recorded trace's steps act on, which decides how each step
    // changes the frame state.
    enum class FrameKind
    {
        Graph,     // BFS, DFS, Dijkstra, A*, Kruskal, the track generator
        Array,      // Merge, Quick and Heap Sort
        Table,       // knapsack DP and refuelling DP (rows x cols); greedy (no table)
        Events,       // the event priority queue
    };

    // Running totals up to and including the current step.
    struct FrameCounters
    {
        long long visited;
        long long examined;
        long long relaxed;
        long long comparisons;
        long long swaps;
        long long tableCells;
        long long heapPushes;
        long long heapPops;
        long long placements;
        long long backtracks;
        long long rejections;
    };

    // Replays a Trace to reconstruct the algorithm's visible state after
    // any step -- the Algorithm Lab's engine. The algorithm itself ran once,
    // for real, recording steps; the cursor never re-runs it.
    //
    // State per kind:
    //   Graph:  vertexState (0 unseen, 1 discovered / in the frontier,
    //           2 visited / settled, 3 finished; the generator stores a role
    //           code instead), vertexValue (distance, discovery time, or
    //           checkpoint order; -1 = none), vertexParent, edgeState (0 not
    //           touched, 1 examined, 2 tree / relaxed / accepted, 3 rejected,
    //           or a MarkEdge code).
    //   Array:  the array's keys after every Swap / Place so far.
    //   Table:  table cells (-1 = not filled yet); `selected` = item ids
    //           taken, `rejected` = item ids skipped.
    //   Events: `selected` = event ids popped and applied, in order;
    //           `rejected` = event ids cancelled.
    //
    // Moving forward applies only the new steps; moving backward replays
    // from the beginning. That is O(steps) for a backward seek -- fine for
    // the Lab's trace sizes, and it keeps the cursor free of per-kind
    // snapshot logic.
    class TraceCursor
    {
    public:
        TraceCursor();

        TraceCursor(const TraceCursor&) = delete;
        TraceCursor& operator=(const TraceCursor&) = delete;

        void Reset(const Trace* trace, FrameKind kind, int vertexCount, int edgeCount,
            const int* initialArray, int arrayLength, int tableRows, int tableCols);

        // Moves to the state after step `step` (clamped to [-1, count - 1];
        // -1 is the state before any step).
        void SeekTo(int step);

        int CurrentStep() const
        {
            return m_step;
        }

        FrameKind Kind() const
        {
            return m_kind;
        }

        const DynArray<int>& VertexState() const
        {
            return m_vertexState;
        }

        const DynArray<double>& VertexValue() const
        {
            return m_vertexValue;
        }

        const DynArray<int>& VertexParent() const
        {
            return m_vertexParent;
        }

        const DynArray<int>& EdgeState() const
        {
            return m_edgeState;
        }

        const DynArray<int>& ArrayValues() const
        {
            return m_array;
        }

        const DynArray<int>& Table() const
        {
            return m_table;
        }

        int TableRows() const
        {
            return m_rows;
        }

        int TableCols() const
        {
            return m_cols;
        }

        const DynArray<int>& Selected() const
        {
            return m_selected;
        }

        const DynArray<int>& Rejected() const
        {
            return m_rejected;
        }

        const FrameCounters& Counters() const
        {
            return m_counters;
        }

    private:
        void Restart();
        void Apply(const TraceStep& step);

        const Trace* m_trace;
        FrameKind m_kind;
        int m_step;
        int m_currentVertex;

        DynArray<int> m_vertexState;
        DynArray<double> m_vertexValue;
        DynArray<int> m_vertexParent;
        DynArray<int> m_edgeState;
        DynArray<int> m_initialArray;
        DynArray<int> m_array;
        DynArray<int> m_table;
        int m_rows;
        int m_cols;
        DynArray<int> m_selected;
        DynArray<int> m_rejected;
        FrameCounters m_counters;
    };
}
