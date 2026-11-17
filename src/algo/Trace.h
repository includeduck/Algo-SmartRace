#pragma once

#include "../core/ds/DynArray.h"

namespace algo
{
    // One recorded step of an algorithm's execution, for the Algorithm Lab
    // to replay. `a`/`b`/`value` mean different things per `op` (see each
    // algorithm's own comments); `line` indexes into that algorithm's
    // pseudocode listing (Phase 4) so the Lab can highlight the active line.
    enum class TraceOp
    {
        VisitVertex,       // a = vertex
        DiscoverVertex,     // a = vertex, b = parent (-1 if none)
        FinishVertex,       // a = vertex
        ExamineEdge,         // a = edgeId
        RelaxEdge,            // a = edgeId, value = new tentative cost
        PushHeap,              // a = vertex or item id
        PopHeap,                // a = vertex or item id
        SetParent,               // a = vertex, b = parent
        DpCell,                   // a = row, b = col, value = cell value
        Compare,                   // a, b = indices/ids compared
        Swap,                        // a, b = indices/ids swapped
        Place,                        // a = slot/vertex, b = value placed
        Backtrack,                     // a = slot/vertex undone
        Reject,                          // a = candidate id, value = reason code
        UnionSets,                        // a, b = set roots unioned
        AcceptEdge,                        // a = edgeId accepted (MST)
        MarkEdge,                            // a = edgeId, b = state code (generator: extra road, shortcut, obstacle)
    };

    struct TraceStep
    {
        TraceOp op;
        int a;
        int b;
        double value;
        int line;
    };

    // A simple, append-only recording of an algorithm's steps. Nullable
    // everywhere it is passed — gameplay code passes nullptr and the
    // algorithm records nothing. The Algorithm Lab (Phase 4) replays this
    // by re-deriving the state at any step from the sequence so far, rather
    // than this class understanding any particular algorithm's semantics.
    class Trace
    {
    public:
        Trace()
        {
        }

        Trace(const Trace&) = delete;
        Trace& operator=(const Trace&) = delete;

        void Record(TraceOp op, int a = -1, int b = -1, double value = 0.0, int line = -1)
        {
            TraceStep step;
            step.op = op;
            step.a = a;
            step.b = b;
            step.value = value;
            step.line = line;
            m_steps.Push(step);
        }

        int StepCount() const
        {
            return m_steps.Size();
        }

        const TraceStep& Step(int index) const
        {
            return m_steps[index];
        }

        void Clear()
        {
            m_steps.Clear();
        }

        // Re-labels steps [firstStep, end) with `line`. Used when one
        // algorithm runs inside another (Kruskal inside the track
        // generator): the steps keep their meaning, but the Lab highlights
        // the OUTER algorithm's pseudocode line.
        void SetLineFrom(int firstStep, int line)
        {
            for (int i = firstStep; i < m_steps.Size(); ++i) m_steps[i].line = line;
        }

        void CopyFrom(const Trace& other)
        {
            m_steps.Clear();
            for (int i = 0; i < other.StepCount(); ++i) m_steps.Push(other.Step(i));
        }

    private:
        DynArray<TraceStep> m_steps;
    };
}
