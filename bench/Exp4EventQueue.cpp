// Experiment 4 -- Event Priority Queue (Project.pdf section 7).
//
// The race engine's own sim::EventQueue (binary min-heap keyed by
// (triggerTime, priority, insertionSequence), lazy-deletion cancel).
// n in {1k, 10k, 100k, 1M} events, four workloads:
//   random        trigger times uniform over [0, 10n) ms, random event types
//   tie_heavy     only 10 distinct trigger times, so most events tie on time
//                 and many on (time, priority): the insertion sequence decides
//   cancel_heavy  random, then half the events are cancelled before processing
//   with_expiry   n/2 timed effects; popping each one schedules its expiry
//                 (insertions DURING extraction, as in a race), n pops in all
// Phases timed separately per run: insert (all Schedule calls), cancel (the
// Cancel calls, cancel_heavy only) and extract (one ProcessUntil draining
// the heap, including skipped cancelled entries and, for with_expiry, the
// expiry insertions). A fresh queue is built for every run, outside the
// timed region; the insert phase includes the queue growing its heap
// array and event pool.
//
// Correctness (untimed, every n and workload): events reach the sink in
// non-decreasing (time, priority, sequence) order; for the workloads
// without expiries the delivered sequence equals a Merge Sort of all
// non-cancelled events by that key; cancelled events are never delivered;
// the delivered count is exact.
#include "BenchUtil.h"
#include "../src/sim/EventQueue.h"
#include "../src/algo/Sorting.h"
#include "../src/core/util/Rng.h"
#include "../src/core/ds/DynArray.h"

#include <cstdio>

namespace
{
    struct Key3
    {
        int time;
        int priority;
        int sequence;
    };

    struct Key3Order
    {
        static int Compare(const Key3& a, const Key3& b)
        {
            if (a.time != b.time) return a.time < b.time ? -1 : 1;
            if (a.priority != b.priority) return a.priority < b.priority ? -1 : 1;
            if (a.sequence != b.sequence) return a.sequence < b.sequence ? -1 : 1;
            return 0;
        }

        static int Key(const Key3& k)
        {
            return k.sequence;
        }
    };

    const sim::EventType kPrimaryTypes[7] =
    {
        sim::EventType::RoadBlock, sim::EventType::RoadClear, sim::EventType::ShortcutToggle, sim::EventType::OilSpill,
        sim::EventType::SpeedRestriction, sim::EventType::FuelStationToggle, sim::EventType::NitroBooster,
    };
    const sim::EventType kTimedTypes[4] =
    {
        sim::EventType::OilSpill, sim::EventType::SpeedRestriction, sim::EventType::FuelStationToggle,
        sim::EventType::NitroBooster,
    };

    sim::EventType ExpiryOf(sim::EventType t)
    {
        switch (t)
        {
            case sim::EventType::OilSpill: return sim::EventType::OilSpillExpire;
            case sim::EventType::SpeedRestriction: return sim::EventType::SpeedRestrictionExpire;
            case sim::EventType::FuelStationToggle: return sim::EventType::FuelStationReactivate;
            case sim::EventType::NitroBooster:
            default: return sim::EventType::NitroBoosterExpire;
        }
    }

    bool IsExpiry(sim::EventType t)
    {
        return t == sim::EventType::OilSpillExpire || t == sim::EventType::SpeedRestrictionExpire
            || t == sim::EventType::FuelStationReactivate || t == sim::EventType::NitroBoosterExpire;
    }

    enum Workload
    {
        kRandom,
        kTieHeavy,
        kCancelHeavy,
        kWithExpiry,
        kWorkloadCount,
    };
    const char* const kWorkloadNames[kWorkloadCount] = { "random", "tie_heavy", "cancel_heavy", "with_expiry" };

    struct PlannedEvent
    {
        sim::EventType type;
        int time;
        int duration;
    };

    // The instance: what to schedule, and which ids to cancel.
    void MakeWorkload(Workload w, int n, unsigned long long seed, DynArray<PlannedEvent>* events, DynArray<int>* cancels)
    {
        Rng rng(seed);
        events->Clear();
        cancels->Clear();
        int count = (w == kWithExpiry) ? n / 2 : n;
        for (int i = 0; i < count; ++i)
        {
            PlannedEvent e;
            if (w == kWithExpiry)
            {
                e.type = kTimedTypes[rng.NextInt(0, 3)];
                e.time = rng.NextInt(0, 10 * n);
                e.duration = rng.NextInt(1000, 30000);
            }
            else
            {
                e.type = kPrimaryTypes[rng.NextInt(0, 6)];
                e.time = (w == kTieHeavy) ? rng.NextInt(0, 9) * 1000 : rng.NextInt(0, 10 * n);
                e.duration = 0;
            }
            events->Push(e);
        }
        if (w == kCancelHeavy)
        {
            for (int i = 0; i < count; ++i)
            {
                if (rng.NextInt(0, 1) == 1) cancels->Push(i);   // ids are assigned 0, 1, 2, ... in schedule order
            }
        }
    }

    // Counts deliveries; schedules expiries for timed effects (with_expiry).
    struct BenchSink : sim::IEventSink
    {
        BenchSink(sim::EventQueue* q, DynArray<Key3>* record)
            : queue(q), delivered(0), log(record)
        {
        }

        void OnEvent(const sim::Event& e) override
        {
            ++delivered;
            if (log != nullptr)
            {
                Key3 k;
                k.time = e.triggerTimeMs;
                k.priority = e.priority;
                k.sequence = e.insertionSequence;
                log->Push(k);
            }
            if (e.durationMs > 0 && !IsExpiry(e.type))
            {
                queue->Schedule(ExpiryOf(e.type), e.triggerTimeMs + e.durationMs, e.target, 0);
            }
        }

        sim::EventQueue* queue;
        int delivered;
        DynArray<Key3>* log;
    };

    void ScheduleAll(sim::EventQueue* q, const DynArray<PlannedEvent>& events)
    {
        for (int i = 0; i < events.Size(); ++i) q->Schedule(events[i].type, events[i].time, i, events[i].duration);
    }

    void CheckOrder(Workload w, int n, const DynArray<PlannedEvent>& events, const DynArray<int>& cancels)
    {
        sim::EventQueue q;
        ScheduleAll(&q, events);
        for (int i = 0; i < cancels.Size(); ++i) q.Cancel(cancels[i]);

        // Reference: every live event's key, merge-sorted.
        DynArray<Key3> reference;
        for (int id = 0; id < events.Size(); ++id)
        {
            const sim::Event* e = q.Find(id);
            if (e == nullptr || e->cancelled) continue;
            Key3 k;
            k.time = e->triggerTimeMs;
            k.priority = e->priority;
            k.sequence = e->insertionSequence;
            reference.Push(k);
        }
        algo::MergeSort<Key3, Key3Order>(reference.Data(), reference.Size());

        DynArray<Key3> delivered;
        BenchSink sink(&q, &delivered);
        q.ProcessUntil(0x7fffffff, &sink);

        bool ordered = true;
        for (int i = 1; i < delivered.Size(); ++i)
        {
            if (Key3Order::Compare(delivered[i - 1], delivered[i]) > 0) ordered = false;
        }

        int expected = events.Size() - cancels.Size() + ((w == kWithExpiry) ? events.Size() : 0);
        bool countOk = (delivered.Size() == expected);
        bool matches = true;
        if (w != kWithExpiry)
        {
            if (reference.Size() != delivered.Size()) matches = false;
            for (int i = 0; i < delivered.Size() && matches; ++i)
            {
                if (Key3Order::Compare(reference[i], delivered[i]) != 0) matches = false;
            }
        }

        CheckLogRecord("exp4", "pop_order", ordered && countOk && matches,
            "%s n=%d: %d delivered (expected %d), %d cancelled, ordered=%s, equals merge-sorted reference=%s",
            kWorkloadNames[w], n, delivered.Size(), expected, cancels.Size(), ordered ? "yes" : "no",
            (w == kWithExpiry) ? "n/a (expiries are created while popping)" : (matches ? "yes" : "no"));
    }
}

void RunExperiment4(const BenchConfig& cfg)
{
    printf("\n=== Experiment 4: Event priority queue ===\n");
    ExperimentOutput out;
    if (!out.Open(cfg, "exp4_event_queue", "phase,workload,n,includes_init",
        "operations,heap_comparisons,heap_swaps,delivered,cancelled_skipped,ns_per_operation")) return;

    const int fullSizes[4] = { 1000, 10000, 100000, 1000000 };
    const int quickSizes[2] = { 1000, 10000 };
    const int* sizes = cfg.quick ? quickSizes : fullSizes;
    int sizeCount = cfg.quick ? 2 : 4;

    double insertSamples[kMaxRuns];
    double cancelSamples[kMaxRuns];
    double extractSamples[kMaxRuns];
    char config[128];
    char counters[200];

    DynArray<PlannedEvent> events;
    DynArray<int> cancels;

    for (int s = 0; s < sizeCount; ++s)
    {
        int n = sizes[s];
        for (int w = 0; w < kWorkloadCount; ++w)
        {
            Workload workload = static_cast<Workload>(w);
            MakeWorkload(workload, n, cfg.seed + static_cast<unsigned long long>(n) * 31 + w, &events, &cancels);
            CheckOrder(workload, n, events, cancels);

            long long insertCmp = 0;
            long long insertSwaps = 0;
            long long extractCmp = 0;
            long long extractSwaps = 0;
            int delivered = 0;

            for (int r = -cfg.warmup; r < cfg.runs; ++r)
            {
                sim::EventQueue* q = new sim::EventQueue();
                BenchSink sink(q, nullptr);

                double t0 = Timer::NowMicros();
                ScheduleAll(q, events);
                double t1 = Timer::NowMicros();
                for (int i = 0; i < cancels.Size(); ++i) q->Cancel(cancels[i]);
                double t2 = Timer::NowMicros();
                long long cmpBefore = q->HeapComparisons();
                long long swapBefore = q->HeapSwaps();
                q->ProcessUntil(0x7fffffff, &sink);
                double t3 = Timer::NowMicros();

                if (r >= 0)
                {
                    insertSamples[r] = t1 - t0;
                    cancelSamples[r] = t2 - t1;
                    extractSamples[r] = t3 - t2;
                }
                insertCmp = cmpBefore;
                insertSwaps = swapBefore;
                extractCmp = q->HeapComparisons() - cmpBefore;
                extractSwaps = q->HeapSwaps() - swapBefore;
                delivered = sink.delivered;
                g_benchSink = g_benchSink + delivered;
                delete q;
            }

            SampleStats ins = ComputeStats(insertSamples, cfg.runs);
            snprintf(config, sizeof(config), "insert,%s,%d,yes", kWorkloadNames[w], n);
            snprintf(counters, sizeof(counters), "%d,%lld,%lld,0,0,%.2f", events.Size(), insertCmp, insertSwaps,
                ins.median * 1000.0 / events.Size());
            out.Emit(cfg, config, 1, insertSamples, counters);

            if (cancels.Size() > 0)
            {
                SampleStats can = ComputeStats(cancelSamples, cfg.runs);
                snprintf(config, sizeof(config), "cancel,%s,%d,no", kWorkloadNames[w], n);
                snprintf(counters, sizeof(counters), "%d,0,0,0,0,%.2f", cancels.Size(),
                    can.median * 1000.0 / cancels.Size());
                out.Emit(cfg, config, 1, cancelSamples, counters);
            }

            // Pops = every entry that was ever in the heap (live, cancelled, expiries).
            int pops = events.Size() + ((workload == kWithExpiry) ? events.Size() : 0);
            SampleStats ext = ComputeStats(extractSamples, cfg.runs);
            snprintf(config, sizeof(config), "extract,%s,%d,no", kWorkloadNames[w], n);
            snprintf(counters, sizeof(counters), "%d,%lld,%lld,%d,%d,%.2f", pops, extractCmp, extractSwaps, delivered,
                cancels.Size(), ext.median * 1000.0 / pops);
            out.Emit(cfg, config, 1, extractSamples, counters);
        }
        printf("  n=%d done\n", n);
    }
}
