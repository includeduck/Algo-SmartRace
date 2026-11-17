#include "TraceCursor.h"

namespace algo
{
    namespace
    {
        bool InRange(int index, int size)
        {
            return index >= 0 && index < size;
        }
    }

    TraceCursor::TraceCursor()
        : m_trace(nullptr), m_kind(FrameKind::Graph), m_step(-1), m_currentVertex(-1), m_rows(0), m_cols(0)
    {
        Restart();
    }

    void TraceCursor::Reset(const Trace* trace, FrameKind kind, int vertexCount, int edgeCount,
        const int* initialArray, int arrayLength, int tableRows, int tableCols)
    {
        m_trace = trace;
        m_kind = kind;

        m_vertexState.Resize(vertexCount > 0 ? vertexCount : 0);
        m_vertexValue.Resize(vertexCount > 0 ? vertexCount : 0);
        m_vertexParent.Resize(vertexCount > 0 ? vertexCount : 0);
        m_edgeState.Resize(edgeCount > 0 ? edgeCount : 0);

        m_initialArray.Clear();
        for (int i = 0; i < arrayLength; ++i) m_initialArray.Push(initialArray[i]);
        m_array.Resize(arrayLength > 0 ? arrayLength : 0);

        m_rows = (tableRows > 0) ? tableRows : 0;
        m_cols = (tableCols > 0) ? tableCols : 0;
        m_table.Resize(m_rows * m_cols);

        Restart();
    }

    void TraceCursor::Restart()
    {
        m_step = -1;
        m_currentVertex = -1;

        for (int i = 0; i < m_vertexState.Size(); ++i)
        {
            m_vertexState[i] = 0;
            m_vertexValue[i] = -1.0;
            m_vertexParent[i] = -1;
        }
        for (int i = 0; i < m_edgeState.Size(); ++i) m_edgeState[i] = 0;
        for (int i = 0; i < m_array.Size(); ++i) m_array[i] = m_initialArray[i];
        for (int i = 0; i < m_table.Size(); ++i) m_table[i] = -1;
        m_selected.Clear();
        m_rejected.Clear();

        m_counters.visited = 0;
        m_counters.examined = 0;
        m_counters.relaxed = 0;
        m_counters.comparisons = 0;
        m_counters.swaps = 0;
        m_counters.tableCells = 0;
        m_counters.heapPushes = 0;
        m_counters.heapPops = 0;
        m_counters.placements = 0;
        m_counters.backtracks = 0;
        m_counters.rejections = 0;
    }

    void TraceCursor::SeekTo(int step)
    {
        int count = (m_trace != nullptr) ? m_trace->StepCount() : 0;
        if (step < -1) step = -1;
        if (step > count - 1) step = count - 1;

        if (step < m_step) Restart();
        while (m_step < step)
        {
            ++m_step;
            Apply(m_trace->Step(m_step));
        }
    }

    void TraceCursor::Apply(const TraceStep& s)
    {
        int vertices = m_vertexState.Size();
        int edges = m_edgeState.Size();

        switch (s.op)
        {
            case TraceOp::VisitVertex:
            {
                ++m_counters.visited;
                m_currentVertex = s.a;
                if (InRange(s.a, vertices)) m_vertexState[s.a] = 2;
                break;
            }
            case TraceOp::DiscoverVertex:
            {
                if (InRange(s.a, vertices))
                {
                    if (m_vertexState[s.a] < 1) m_vertexState[s.a] = 1;
                    m_vertexParent[s.a] = s.b;
                    m_vertexValue[s.a] = s.value;
                }
                break;
            }
            case TraceOp::FinishVertex:
            {
                if (InRange(s.a, vertices)) m_vertexState[s.a] = 3;
                break;
            }
            case TraceOp::ExamineEdge:
            {
                ++m_counters.examined;
                if (InRange(s.a, edges) && m_edgeState[s.a] < 1) m_edgeState[s.a] = 1;
                break;
            }
            case TraceOp::RelaxEdge:
            {
                ++m_counters.relaxed;
                if (InRange(s.a, edges)) m_edgeState[s.a] = 2;
                if (InRange(s.b, vertices))
                {
                    m_vertexValue[s.b] = s.value;
                    m_vertexParent[s.b] = m_currentVertex;
                    if (m_vertexState[s.b] == 0) m_vertexState[s.b] = 1;
                }
                break;
            }
            case TraceOp::PushHeap:
            {
                ++m_counters.heapPushes;
                if (m_kind == FrameKind::Graph && InRange(s.a, vertices) && m_vertexState[s.a] == 0)
                {
                    m_vertexState[s.a] = 1;
                }
                break;
            }
            case TraceOp::PopHeap:
            {
                ++m_counters.heapPops;
                if (m_kind == FrameKind::Events) m_selected.Push(s.a);
                break;
            }
            case TraceOp::SetParent:
            {
                if (InRange(s.a, vertices)) m_vertexParent[s.a] = s.b;
                break;
            }
            case TraceOp::DpCell:
            {
                ++m_counters.tableCells;
                if (InRange(s.a, m_rows) && InRange(s.b, m_cols)) m_table[s.a * m_cols + s.b] = static_cast<int>(s.value);
                break;
            }
            case TraceOp::Compare:
            {
                ++m_counters.comparisons;
                break;
            }
            case TraceOp::Swap:
            {
                ++m_counters.swaps;
                if (m_kind == FrameKind::Array && InRange(s.a, m_array.Size()) && InRange(s.b, m_array.Size()))
                {
                    int t = m_array[s.a];
                    m_array[s.a] = m_array[s.b];
                    m_array[s.b] = t;
                }
                break;
            }
            case TraceOp::Place:
            {
                ++m_counters.placements;
                if (m_kind == FrameKind::Array)
                {
                    if (InRange(s.a, m_array.Size())) m_array[s.a] = s.b;
                }
                else if (m_kind == FrameKind::Graph)
                {
                    if (InRange(s.a, vertices))
                    {
                        m_vertexState[s.a] = s.b;
                        m_vertexValue[s.a] = s.value;
                    }
                }
                else
                {
                    m_selected.Push(s.b);
                }
                break;
            }
            case TraceOp::Backtrack:
            {
                ++m_counters.backtracks;
                if (InRange(s.a, vertices))
                {
                    m_vertexState[s.a] = 0;
                    m_vertexValue[s.a] = -1.0;
                }
                break;
            }
            case TraceOp::Reject:
            {
                ++m_counters.rejections;
                if (m_kind == FrameKind::Graph)
                {
                    if (InRange(s.a, edges)) m_edgeState[s.a] = 3;
                }
                else
                {
                    m_rejected.Push(s.a);
                }
                break;
            }
            case TraceOp::AcceptEdge:
            {
                if (InRange(s.a, edges)) m_edgeState[s.a] = 2;
                break;
            }
            case TraceOp::MarkEdge:
            {
                if (InRange(s.a, edges)) m_edgeState[s.a] = s.b;
                break;
            }
            case TraceOp::UnionSets:
            default:
            {
                break;
            }
        }
    }
}
