#include "TestFramework.h"

#include "../src/sim/EventQueue.h"
#include "../src/sim/EffectManager.h"
#include "../src/model/Graph.h"
#include "../src/core/util/MathUtil.h"

using namespace sim;
using namespace model;

namespace
{
    struct RecordingSink : IEventSink
    {
        RecordingSink()
            : count(0)
        {
        }

        void OnEvent(const Event& event) override
        {
            if (count < 32)
            {
                order[count] = event.eventId;
                types[count] = event.type;
                ++count;
            }
        }

        int order[32];
        EventType types[32];
        int count;
    };
}

TEST(EventQueue_OrdersByTriggerTimeThenPriorityThenSequence)
{
    EventQueue q;
    // Two events at the SAME trigger time: RoadBlock (priority 1) must fire
    // before NitroBooster (priority 6), regardless of schedule order.
    int nitroId = q.Schedule(EventType::NitroBooster, 1000, 5, 25000);
    int blockId = q.Schedule(EventType::RoadBlock, 1000, 2, 0);
    // A third event scheduled later in triggerTime fires last.
    int laterId = q.Schedule(EventType::RoadClear, 2000, 2, 0);

    RecordingSink sink;
    q.ProcessUntil(2000, &sink);

    CHECK_EQ(sink.count, 3);
    CHECK_EQ(sink.order[0], blockId);
    CHECK_EQ(sink.order[1], nitroId);
    CHECK_EQ(sink.order[2], laterId);
}

TEST(EventQueue_SimultaneousEventsSameTypeTiebreakBySequence)
{
    EventQueue q;
    int first = q.Schedule(EventType::OilSpill, 500, 1, 20000);
    int second = q.Schedule(EventType::OilSpill, 500, 2, 20000);

    RecordingSink sink;
    q.ProcessUntil(500, &sink);

    CHECK_EQ(sink.count, 2);
    CHECK_EQ(sink.order[0], first);
    CHECK_EQ(sink.order[1], second);
}

TEST(EventQueue_ProcessUntil_OnlyFiresDueEvents)
{
    EventQueue q;
    q.Schedule(EventType::RoadBlock, 5000, 0, 0);

    RecordingSink sink;
    q.ProcessUntil(1000, &sink);   // not due yet
    CHECK_EQ(sink.count, 0);
    CHECK_EQ(q.PendingCount(), 1);

    q.ProcessUntil(5000, &sink);
    CHECK_EQ(sink.count, 1);
    CHECK_EQ(q.PendingCount(), 0);
}

TEST(EventQueue_Cancel_LazyDeletion_SkippedWhenPopped)
{
    EventQueue q;
    int a = q.Schedule(EventType::RoadBlock, 1000, 0, 0);
    int b = q.Schedule(EventType::RoadClear, 1000, 0, 0);
    (void)a;

    q.Cancel(b);
    CHECK_EQ(q.PendingCount(), 1);   // a is still pending, b was cancelled before firing

    RecordingSink sink;
    q.ProcessUntil(1000, &sink);
    CHECK_EQ(sink.count, 1);   // only a actually fired
    CHECK_EQ(sink.order[0], a);
}

TEST(EventQueue_CancelAfterFiring_IsHarmless)
{
    EventQueue q;
    int a = q.Schedule(EventType::RoadBlock, 1000, 0, 0);

    RecordingSink sink;
    q.ProcessUntil(1000, &sink);
    CHECK_EQ(sink.count, 1);

    q.Cancel(a);   // already fired -- cancelling now must not crash or double-fire
    CHECK_EQ(q.PendingCount(), 0);
}

TEST(EffectManager_OverlappingOilSpills_ExpiringOneKeepsTheOtherActive)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(1, 0, VertexType::Finish);
    int e = g.AddEdge(a, b, 10.0, 1.0, 1.0, 1, false, false);

    EffectManager effects(&g);
    int effect1 = effects.AddEdgeEffect(e, EffectType::OilSpill, 2.0, 20000);
    int effect2 = effects.AddEdgeEffect(e, EffectType::OilSpill, 2.0, 25000);

    CHECK_EQ(g.GetEdge(e).effDifficulty, 3);   // 1 base + max(2,2) bonus
    CHECK_EQ(static_cast<int>(g.GetEdge(e).status), static_cast<int>(EdgeStatus::Restricted));

    effects.RemoveEdgeEffect(e, effect1);
    CHECK_EQ(g.GetEdge(e).effDifficulty, 3);   // effect2 still active, same bonus
    CHECK_EQ(static_cast<int>(g.GetEdge(e).status), static_cast<int>(EdgeStatus::Restricted));

    effects.RemoveEdgeEffect(e, effect2);
    CHECK_EQ(g.GetEdge(e).effDifficulty, 1);   // both gone, back to base
    CHECK_EQ(static_cast<int>(g.GetEdge(e).status), static_cast<int>(EdgeStatus::Open));
}

TEST(EffectManager_SpeedRestriction_TakesTheStrictestCap)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(1, 0, VertexType::Finish);
    int e = g.AddEdge(a, b, 100.0, 2.0, 1.0, 1, false, false);   // base speed = 50

    EffectManager effects(&g);
    int loose = effects.AddEdgeEffect(e, EffectType::SpeedRestriction, 0.8, 20000);   // cap at 40
    effects.AddEdgeEffect(e, EffectType::SpeedRestriction, 0.5, 25000);                 // cap at 25 (stricter)

    CHECK(MathUtil::Abs(g.GetEdge(e).speedCap - 25.0) < 1e-9);

    effects.RemoveEdgeEffect(e, loose);
    CHECK(MathUtil::Abs(g.GetEdge(e).speedCap - 25.0) < 1e-9);   // the stricter one is still active
}

TEST(EffectManager_RoadBlock_OverridesEvenWithNoOtherEffects)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(1, 0, VertexType::Finish);
    int e = g.AddEdge(a, b, 1.0, 1.0, 1.0, 1, false, false);

    EffectManager effects(&g);
    CHECK(g.IsTraversable(e));
    effects.SetEdgeBlocked(e, true);
    CHECK(!g.IsTraversable(e));
    effects.SetEdgeBlocked(e, false);
    CHECK(g.IsTraversable(e));
}

TEST(EffectManager_ShortcutStartsBlocked_ToggleOpensIt)
{
    Graph g;
    int a = g.AddVertex(0, 0, VertexType::Start);
    int b = g.AddVertex(1, 0, VertexType::Finish);
    int e = g.AddEdge(a, b, 1.0, 1.0, 1.0, 5, true, false, EdgeStatus::Blocked);

    EffectManager effects(&g);
    CHECK(!g.IsTraversable(e));   // starts blocked per the .trk's base status
    effects.SetShortcutOpen(e, true);
    CHECK(g.IsTraversable(e));
    effects.SetShortcutOpen(e, false);
    CHECK(!g.IsTraversable(e));
}
