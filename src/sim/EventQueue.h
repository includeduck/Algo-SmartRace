#pragma once

#include "Event.h"
#include "../core/ds/MinHeap.h"
#include "../core/ds/Pool.h"
#include "../core/ds/DynArray.h"

namespace sim
{
    struct EventLess
    {
        static bool Less(Event* const& a, Event* const& b)
        {
            if (a->triggerTimeMs != b->triggerTimeMs) return a->triggerTimeMs < b->triggerTimeMs;
            if (a->priority != b->priority) return a->priority < b->priority;
            return a->insertionSequence < b->insertionSequence;
        }
    };

    // The engine implements this to react to events as they fire, so
    // EventQueue itself knows nothing about edges, vertices or effects.
    class IEventSink
    {
    public:
        virtual ~IEventSink()
        {
        }

        virtual void OnEvent(const Event& event) = 0;
    };

    // Min-heap keyed by (triggerTimeMs, priority, insertionSequence), all
    // integers, so ordering never depends on floating-point equality
    // (Project.pdf section 4.5). Cancellation is lazy deletion: Cancel()
    // just marks the event; ProcessUntil skips cancelled entries when they
    // reach the heap top. Trade-off: O(1) cancel and a simple implementation,
    // at the cost of a cancelled entry sitting in the heap (and the pool)
    // until its turn comes up — bounded by the total number of events ever
    // scheduled in one race, which this project's circuits keep small.
    class EventQueue
    {
    public:
        EventQueue();
        ~EventQueue();
        EventQueue(const EventQueue&) = delete;
        EventQueue& operator=(const EventQueue&) = delete;

        // `target` must already be validated (edge/vertex exists, is the
        // right kind for `type`) by the caller — EventQueue only orders
        // and stores events, it does not know what a "vertex" is.
        int Schedule(EventType type, int triggerTimeMs, int target, int durationMs, int linkedEffectId = -1);

        void Cancel(int eventId);

        // Pops and applies every event with triggerTimeMs <= now, in heap
        // order (ties broken by priority then insertion sequence), calling
        // sink->OnEvent for each. Brief: "apply all events with the same
        // trigger time in heap order before recalculating routes" — the
        // caller recalculates once after this returns, not per event.
        void ProcessUntil(int nowMs, IEventSink* sink);

        bool Empty() const;

        // The event with this id (nullptr if none), whether pending, fired
        // or cancelled.
        const Event* Find(int eventId) const;

        // Live (not cancelled, not yet fired) events still queued. O(n) —
        // diagnostic/test use only.
        int PendingCount() const;

        // The heap's own operation counters since construction (sift
        // comparisons and element swaps) -- Experiment 4's "heap operations".
        long long HeapComparisons() const
        {
            return m_heap.comparisons;
        }

        long long HeapSwaps() const
        {
            return m_heap.swaps;
        }

    private:
        MinHeap<Event*, EventLess> m_heap;
        Pool<Event> m_pool;
        DynArray<Event*> m_byId;   // eventId -> Event*, for O(1) Cancel
        int m_nextEventId;
        int m_nextSequence;
    };
}
