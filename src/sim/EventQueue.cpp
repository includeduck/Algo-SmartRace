#include "EventQueue.h"

namespace sim
{
    namespace
    {
        int PriorityForType(EventType type)
        {
            switch (type)
            {
                case EventType::RoadBlock:
                case EventType::RoadClear:
                    return 1;
                case EventType::ShortcutToggle:
                    return 2;
                case EventType::OilSpill:
                case EventType::OilSpillExpire:
                    return 3;
                case EventType::SpeedRestriction:
                case EventType::SpeedRestrictionExpire:
                    return 4;
                case EventType::FuelStationToggle:
                case EventType::FuelStationReactivate:
                    return 5;
                case EventType::NitroBooster:
                case EventType::NitroBoosterExpire:
                default:
                    return 6;
            }
        }
    }

    EventQueue::EventQueue()
        : m_pool(256), m_nextEventId(0), m_nextSequence(0)
    {
    }

    EventQueue::~EventQueue()
    {
    }

    int EventQueue::Schedule(EventType type, int triggerTimeMs, int target, int durationMs, int linkedEffectId)
    {
        Event* e = m_pool.Allocate();
        e->eventId = m_nextEventId;
        ++m_nextEventId;
        e->type = type;
        e->triggerTimeMs = triggerTimeMs;
        e->priority = PriorityForType(type);
        e->insertionSequence = m_nextSequence;
        ++m_nextSequence;
        e->target = target;
        e->durationMs = durationMs;
        e->effectId = linkedEffectId;
        e->cancelled = false;
        e->fired = false;

        m_byId.Push(e);   // relies on eventId being assigned 0,1,2,... matching push order
        m_heap.Push(e);

        return e->eventId;
    }

    void EventQueue::Cancel(int eventId)
    {
        if (eventId < 0 || eventId >= m_byId.Size()) return;
        m_byId[eventId]->cancelled = true;
    }

    void EventQueue::ProcessUntil(int nowMs, IEventSink* sink)
    {
        while (!m_heap.Empty() && m_heap.Top()->triggerTimeMs <= nowMs)
        {
            Event* e = m_heap.Pop();
            e->fired = true;
            if (e->cancelled) continue;
            sink->OnEvent(*e);
        }
    }

    bool EventQueue::Empty() const
    {
        return m_heap.Empty();
    }

    const Event* EventQueue::Find(int eventId) const
    {
        if (eventId < 0 || eventId >= m_byId.Size()) return nullptr;
        return m_byId[eventId];
    }

    int EventQueue::PendingCount() const
    {
        int count = 0;
        for (int i = 0; i < m_byId.Size(); ++i)
        {
            if (!m_byId[i]->fired && !m_byId[i]->cancelled) ++count;
        }
        return count;
    }
}
