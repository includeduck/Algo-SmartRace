#include "RaceEngine.h"
#include "ResourcePlanner.h"
#include "../algo/RoutePlanner.h"
#include "../algo/Comparators.h"
#include "../algo/Sorting.h"
#include "../model/Rules.h"
#include "../core/util/String.h"
#include "../core/util/MathUtil.h"

#include <cstdio>
#include <cstdarg>

namespace sim
{
    namespace
    {
        bool ParseEventType(const char* s, EventType* out)
        {
            if (String::StrEquals(s, "RoadBlock"))
            {
                *out = EventType::RoadBlock;
                return true;
            }
            if (String::StrEquals(s, "RoadClear"))
            {
                *out = EventType::RoadClear;
                return true;
            }
            if (String::StrEquals(s, "ShortcutToggle"))
            {
                *out = EventType::ShortcutToggle;
                return true;
            }
            if (String::StrEquals(s, "OilSpill"))
            {
                *out = EventType::OilSpill;
                return true;
            }
            if (String::StrEquals(s, "SpeedRestriction"))
            {
                *out = EventType::SpeedRestriction;
                return true;
            }
            if (String::StrEquals(s, "FuelStationToggle"))
            {
                *out = EventType::FuelStationToggle;
                return true;
            }
            if (String::StrEquals(s, "NitroBooster"))
            {
                *out = EventType::NitroBooster;
                return true;
            }
            return false;
        }
    }

    RaceEngine::RaceEngine()
        : m_graph(nullptr), m_effects(nullptr), m_events(nullptr),
          m_humanPreset(model::Preset::Balanced), m_humanAutopilot(true), m_humanIndex(0),
          m_laps(Rules::kDefaultLaps), m_nowMs(0), m_raceOver(false),
          m_graphChangedThisBatch(false), m_leaderFinishMs(-1), m_totalCheckpoints(0),
          m_pendingEdgeChoice(-1), m_startVertex(-1), m_finishVertex(-1)
    {
        m_lastCommandError[0] = '\0';
    }

    RaceEngine::~RaceEngine()
    {
        delete m_effects;
        delete m_events;
        for (int i = 0; i < m_strategies.Size(); ++i) delete m_strategies[i];
        for (int i = 0; i < m_detours.Size(); ++i) delete m_detours[i];
    }

    void RaceEngine::Start(model::Graph* graph, const io::TrackData& trackData, const RaceConfig& config)
    {
        m_graph = graph;
        delete m_effects;
        m_effects = new EffectManager(graph);
        delete m_events;
        m_events = new EventQueue();

        m_laps = config.laps;
        m_humanPreset = config.humanPreset;
        m_humanWeights = model::WeightsForPreset(config.humanPreset);
        m_humanAutopilot = config.humanAutopilot;
        m_costReferences = model::ComputeReferences(*graph);

        m_nowMs = 0;
        m_raceOver = false;
        m_graphChangedThisBatch = false;
        m_leaderFinishMs = -1;
        m_pendingEdgeChoice = -1;
        m_lastCommandError[0] = '\0';
        m_log.Clear();

        m_startVertex = -1;
        m_finishVertex = -1;
        m_totalCheckpoints = 0;
        for (int i = 0; i < graph->VertexCount(); ++i)
        {
            const model::Vertex& v = graph->GetVertex(i);
            if (v.type == model::VertexType::Start) m_startVertex = i;
            if (v.type == model::VertexType::Finish) m_finishVertex = i;
            if (v.checkpointOrder >= 0) ++m_totalCheckpoints;
        }

        m_mainRoute.Clear();
        for (int i = 0; i < trackData.mainRoute.Size(); ++i) m_mainRoute.Push(trackData.mainRoute[i]);

        for (int i = 0; i < m_detours.Size(); ++i) delete m_detours[i];
        m_detours.Clear();
        for (int i = 0; i < trackData.detours.Size(); ++i)
        {
            const io::DetourRule* src = trackData.detours[i];
            io::DetourRule* copy = new io::DetourRule();
            copy->blockedEdgeId = src->blockedEdgeId;
            for (int k = 0; k < src->viaVertices.Size(); ++k) copy->viaVertices.Push(src->viaVertices[k]);
            m_detours.Push(copy);
        }

        m_shortcutPreference.Clear();
        for (int i = 0; i < trackData.shortcutPreference.Size(); ++i)
        {
            m_shortcutPreference.Push(trackData.shortcutPreference[i]);
        }

        for (int i = 0; i < m_strategies.Size(); ++i) delete m_strategies[i];
        m_strategies.Clear();
        m_racers.Clear();

        int racerCount = 1 + config.opponentCount;
        m_humanIndex = 0;

        for (int i = 0; i < racerCount; ++i)
        {
            model::Racer racer;
            racer.id = i;
            if (i == 0)
            {
                String::StrCopy(racer.name, sizeof(racer.name), "You");
            }
            else
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "Racer %c", static_cast<char>('A' + ((i - 1) % 26)));
                String::StrCopy(racer.name, sizeof(racer.name), buf);
            }
            racer.isHuman = (i == 0);
            racer.strategyType = (i == 0) ? model::StrategyType::Human
                : static_cast<model::StrategyType>(1 + ((i - 1) % 3));

            racer.state = model::RacerState::AtVertex;
            racer.currentVertex = m_startVertex;
            racer.currentEdge = -1;
            racer.currentTargetVertex = -1;
            racer.edgeProgress = 0.0;
            racer.fuelTenths = Rules::kMaxFuelTenths;
            racer.fuelRemainder = 0.0;
            racer.refuelledAtVertex = -1;
            racer.nitroCharges = config.startNitroCharges;
            racer.nitroActive = false;
            racer.nitroTimeLeftMs = 0;
            racer.lap = 1;
            racer.nextCheckpoint = 1;
            racer.checkpointsCompletedThisLap = 0;
            racer.totalTimeMs = 0;
            racer.finishTimeMs = -1;
            racer.waitingStartMs = -1;
            racer.refuelEndMs = -1;
            racer.bestLapTimeMs = 0x7fffffff;
            racer.currentLapStartMs = 0;
            racer.plannedVertexPathLen = 0;
            racer.plannedEdgePathLen = 0;
            racer.plannedEdgeCursor = 0;

            m_racers.Push(racer);

            IStrategy* strategy = nullptr;
            if (i != 0)
            {
                switch (static_cast<model::StrategyType>(1 + ((i - 1) % 3)))
                {
                    case model::StrategyType::FixedRoute: strategy = new FixedRouteStrategy(); break;
                    case model::StrategyType::ShortcutLover: strategy = new ShortcutLoverStrategy(); break;
                    case model::StrategyType::ShortestPath:
                    default: strategy = new ShortestPathStrategy(); break;
                }
            }
            m_strategies.Push(strategy);
        }

        m_needsReplan.Clear();
        for (int i = 0; i < m_racers.Size(); ++i) m_needsReplan.Push(true);

        m_pendingNitroExpireEventId.Clear();
        for (int i = 0; i < graph->VertexCount(); ++i) m_pendingNitroExpireEventId.Push(-1);

        (void)config.eventSeed;   // Phase 2 plays the authored .trk script as-is
        for (int i = 0; i < trackData.events.Size(); ++i)
        {
            const io::ScriptedEvent& se = trackData.events[i];
            EventType type;
            if (ParseEventType(se.type.CStr(), &type))
            {
                m_events->Schedule(type, se.triggerTimeMs, se.target, se.durationMs);
            }
            else
            {
                LogLine("Unknown scripted event type '%s' ignored", se.type.CStr());
            }
        }

        LogLine("Race started: %d racer(s), %d lap(s)", m_racers.Size(), m_laps);
    }

    void RaceEngine::LogLine(const char* fmt, ...)
    {
        char buffer[200];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);
        m_log.Write(LogLevel::Info, m_nowMs, "%s", buffer);
    }

    namespace
    {
        // Event-target validation (Project.pdf 4.5: "validate that an
        // event target exists and that the event is valid in the current
        // state"): the id must exist AND be the right kind of element.
        bool VertexIs(const model::Graph& g, int v, model::VertexType type)
        {
            return v >= 0 && v < g.VertexCount() && g.GetVertex(v).type == type;
        }
    }

    void RaceEngine::OnEvent(const Event& event)
    {
        switch (event.type)
        {
            case EventType::RoadBlock:
                if (event.target >= 0 && event.target < m_graph->EdgeCount())
                {
                    m_effects->SetEdgeBlocked(event.target, true);
                    LogLine("Road blocked: edge %d", event.target);
                    m_graphChangedThisBatch = true;
                }
                else
                {
                    LogLine("RoadBlock rejected: edge %d does not exist", event.target);
                }
                break;

            case EventType::RoadClear:
                if (event.target >= 0 && event.target < m_graph->EdgeCount())
                {
                    m_effects->SetEdgeBlocked(event.target, false);
                    LogLine("Road cleared: edge %d", event.target);
                    m_graphChangedThisBatch = true;
                }
                else
                {
                    LogLine("RoadClear rejected: edge %d does not exist", event.target);
                }
                break;

            case EventType::ShortcutToggle:
                if (event.target >= 0 && event.target < m_graph->EdgeCount() && m_graph->GetEdge(event.target).isShortcut)
                {
                    bool currentlyOpen = m_graph->IsTraversable(event.target);
                    m_effects->SetShortcutOpen(event.target, !currentlyOpen);
                    LogLine("Shortcut %s: edge %d", currentlyOpen ? "closed" : "opened", event.target);
                    m_graphChangedThisBatch = true;
                }
                else
                {
                    LogLine("ShortcutToggle rejected: edge %d is not a shortcut", event.target);
                }
                break;

            case EventType::OilSpill:
                if (event.target >= 0 && event.target < m_graph->EdgeCount())
                {
                    int expiresAt = event.triggerTimeMs + event.durationMs;
                    int effectId = m_effects->AddEdgeEffect(event.target, EffectType::OilSpill,
                        Rules::kOilSpillDifficultyBonus, expiresAt);
                    m_events->Schedule(EventType::OilSpillExpire, expiresAt, event.target, 0, effectId);
                    LogLine("Oil spill on edge %d", event.target);
                    m_graphChangedThisBatch = true;
                }
                else
                {
                    LogLine("OilSpill rejected: edge %d does not exist", event.target);
                }
                break;

            case EventType::OilSpillExpire:
                if (event.target >= 0 && event.target < m_graph->EdgeCount())
                {
                    m_effects->RemoveEdgeEffect(event.target, event.effectId);
                    LogLine("Oil spill cleared on edge %d", event.target);
                    m_graphChangedThisBatch = true;
                }
                break;

            case EventType::SpeedRestriction:
                if (event.target >= 0 && event.target < m_graph->EdgeCount())
                {
                    int expiresAt = event.triggerTimeMs + event.durationMs;
                    int effectId = m_effects->AddEdgeEffect(event.target, EffectType::SpeedRestriction,
                        Rules::kSpeedRestrictionCapFraction, expiresAt);
                    m_events->Schedule(EventType::SpeedRestrictionExpire, expiresAt, event.target, 0, effectId);
                    LogLine("Speed restriction on edge %d", event.target);
                    m_graphChangedThisBatch = true;
                }
                else
                {
                    LogLine("SpeedRestriction rejected: edge %d does not exist", event.target);
                }
                break;

            case EventType::SpeedRestrictionExpire:
                if (event.target >= 0 && event.target < m_graph->EdgeCount())
                {
                    m_effects->RemoveEdgeEffect(event.target, event.effectId);
                    LogLine("Speed restriction cleared on edge %d", event.target);
                    m_graphChangedThisBatch = true;
                }
                break;

            case EventType::FuelStationToggle:
                if (VertexIs(*m_graph, event.target, model::VertexType::FuelStation))
                {
                    m_effects->SetVertexActive(event.target, false);
                    m_events->Schedule(EventType::FuelStationReactivate,
                        event.triggerTimeMs + event.durationMs, event.target, 0);
                    LogLine("Fuel station deactivated: vertex %d", event.target);
                    m_graphChangedThisBatch = true;
                }
                else
                {
                    LogLine("FuelStationToggle rejected: vertex %d is not a fuel station", event.target);
                }
                break;

            case EventType::FuelStationReactivate:
                if (VertexIs(*m_graph, event.target, model::VertexType::FuelStation))
                {
                    m_effects->SetVertexActive(event.target, true);
                    LogLine("Fuel station reactivated: vertex %d", event.target);
                    m_graphChangedThisBatch = true;
                }
                break;

            case EventType::NitroBooster:
                if (VertexIs(*m_graph, event.target, model::VertexType::NitroPickup))
                {
                    m_effects->SetVertexActive(event.target, true);
                    int expireId = m_events->Schedule(EventType::NitroBoosterExpire,
                        event.triggerTimeMs + event.durationMs, event.target, 0);
                    m_pendingNitroExpireEventId[event.target] = expireId;
                    LogLine("Nitro booster active: vertex %d", event.target);
                }
                else
                {
                    LogLine("NitroBooster rejected: vertex %d is not a nitro pickup", event.target);
                }
                break;

            case EventType::NitroBoosterExpire:
                if (VertexIs(*m_graph, event.target, model::VertexType::NitroPickup))
                {
                    m_effects->SetVertexActive(event.target, false);
                    m_pendingNitroExpireEventId[event.target] = -1;
                    LogLine("Nitro booster expired: vertex %d", event.target);
                }
                break;
        }
    }

    void RaceEngine::Step()
    {
        if (m_raceOver) return;

        m_nowMs += Rules::kSimStepMs;
        m_graphChangedThisBatch = false;
        m_events->ProcessUntil(m_nowMs, this);

        if (m_graphChangedThisBatch)
        {
            InvalidateStalePlans();
            for (int i = 0; i < m_needsReplan.Size(); ++i) m_needsReplan[i] = true;
            LogLine("Route recalculation triggered at t=%dms", m_nowMs);
        }

        for (int i = 0; i < m_racers.Size(); ++i) StepRacer(i);

        CheckRaceOverConditions();
    }

    void RaceEngine::StepRacer(int index)
    {
        model::Racer& racer = m_racers[index];

        switch (racer.state)
        {
            case model::RacerState::OnEdge:
                AdvanceOnEdge(racer, index);
                break;
            case model::RacerState::Refuelling:
                if (m_nowMs >= racer.refuelEndMs)
                {
                    racer.state = model::RacerState::AtVertex;
                    racer.refuelEndMs = -1;
                    TryStartNextEdge(racer, index);
                }
                break;
            case model::RacerState::AtVertex:
                TryStartNextEdge(racer, index);
                break;
            case model::RacerState::Waiting:
                ApplyRecoveryCheck(racer, index);
                break;
            case model::RacerState::Finished:
            case model::RacerState::Dnf:
                break;
        }
    }

    double RaceEngine::EdgeTravelSpeed(int edgeId, bool nitroActive) const
    {
        const model::Edge& e = m_graph->GetEdge(edgeId);
        double baseSpeed = (e.baseTime > 0.0) ? (e.distance / e.baseTime) : 1.0;
        double eff = baseSpeed * (1.0 - 0.08 * (e.effDifficulty - e.difficulty));
        if (e.speedCap > 0.0) eff = MathUtil::Min(eff, e.speedCap);
        if (eff <= 0.0) eff = baseSpeed * 0.01;
        if (nitroActive) eff *= Rules::kNitroSpeedMultiplier;
        return eff;
    }

    int RaceEngine::EdgeFuelTenthsNeeded(int edgeId) const
    {
        return EdgeFuelTenths(*m_graph, edgeId);   // the one fuel rule, shared with the planner
    }

    void RaceEngine::AdvanceOnEdge(model::Racer& racer, int index)
    {
        const model::Edge& edge = m_graph->GetEdge(racer.currentEdge);
        double speed = EdgeTravelSpeed(racer.currentEdge, racer.nitroActive);
        double dtSec = Rules::kSimStepMs / 1000.0;
        double progressDelta = (edge.distance > 0.0) ? (speed * dtSec / edge.distance) : 1.0;

        // Fuel is charged in proportion to the distance actually covered:
        // the final tick usually overshoots 1.0, and that overshoot is not
        // charged. Each tick's share is a small fraction of a tenth, so it
        // is accumulated in fuelRemainder and only whole tenths are
        // deducted -- rounding every 20ms slice on its own (the Phase 2
        // behaviour) rounded most slices to zero and barely drained the
        // tank at all. The arithmetic is identical every run, so this stays
        // deterministic.
        double coveredFraction = MathUtil::Min(progressDelta, 1.0 - racer.edgeProgress);
        double fuelForWholeEdge = static_cast<double>(EdgeFuelTenthsNeeded(racer.currentEdge));
        double fuelMultiplier = racer.nitroActive ? Rules::kNitroFuelMultiplier : 1.0;

        racer.fuelRemainder += fuelForWholeEdge * coveredFraction * fuelMultiplier;
        int wholeTenths = static_cast<int>(racer.fuelRemainder);
        racer.fuelTenths -= wholeTenths;
        racer.fuelRemainder -= wholeTenths;
        if (racer.fuelTenths < 0) racer.fuelTenths = 0;

        racer.edgeProgress += progressDelta;
        racer.totalTimeMs += Rules::kSimStepMs;

        if (racer.nitroActive)
        {
            racer.nitroTimeLeftMs -= Rules::kSimStepMs;
            if (racer.nitroTimeLeftMs <= 0)
            {
                racer.nitroActive = false;
                racer.nitroTimeLeftMs = 0;
            }
        }

        if (racer.edgeProgress >= 1.0)
        {
            racer.edgeProgress = 0.0;
            ArriveAtVertex(racer, index);
        }
    }

    void RaceEngine::ArriveAtVertex(model::Racer& racer, int index)
    {
        int vertexId = racer.currentTargetVertex;
        racer.currentVertex = vertexId;
        racer.currentEdge = -1;
        racer.currentTargetVertex = -1;
        racer.state = model::RacerState::AtVertex;

        const model::Vertex& v = m_graph->GetVertex(vertexId);

        if (v.checkpointOrder == racer.nextCheckpoint)
        {
            ++racer.nextCheckpoint;
            ++racer.checkpointsCompletedThisLap;
        }

        if (vertexId == m_finishVertex && racer.nextCheckpoint > m_totalCheckpoints)
        {
            int lapTimeMs = m_nowMs - racer.currentLapStartMs;
            if (lapTimeMs < racer.bestLapTimeMs) racer.bestLapTimeMs = lapTimeMs;

            if (racer.lap >= m_laps)
            {
                racer.state = model::RacerState::Finished;
                racer.finishTimeMs = m_nowMs;
                if (m_leaderFinishMs < 0) m_leaderFinishMs = m_nowMs;
                LogLine("%s finished (t=%dms, best lap %dms)", racer.name, m_nowMs, racer.bestLapTimeMs);
                return;
            }

            ++racer.lap;
            racer.nextCheckpoint = 1;
            racer.checkpointsCompletedThisLap = 0;
            racer.currentLapStartMs = m_nowMs;
            LogLine("%s started lap %d", racer.name, racer.lap);
        }

        if (racer.state == model::RacerState::AtVertex) TryStartNextEdge(racer, index);
    }

    // Collects an active nitro pickup / takes an opportunistic opponent
    // refuel at the racer's CURRENT vertex. Called both on arrival
    // (ArriveAtVertex) and every tick a racer sits AtVertex before it
    // decides its next edge (TryStartNextEdge) -- a pickup can activate
    // via a scripted event WHILE a racer is already parked there (the
    // clearest case: a nitro pickup at the Start vertex, collected on the
    // very first tick, before any "arrival" ever happens), not only when
    // a racer drives onto it.
    void RaceEngine::CheckVertexPickups(model::Racer& racer, int index)
    {
        if (racer.currentVertex < 0) return;
        const model::Vertex& v = m_graph->GetVertex(racer.currentVertex);

        if (v.type == model::VertexType::NitroPickup && v.active && racer.nitroCharges < Rules::kMaxNitroCharges)
        {
            ++racer.nitroCharges;
            m_effects->SetVertexActive(racer.currentVertex, false);
            int pendingId = m_pendingNitroExpireEventId[racer.currentVertex];
            if (pendingId >= 0)
            {
                m_events->Cancel(pendingId);
                m_pendingNitroExpireEventId[racer.currentVertex] = -1;
            }
            LogLine("%s collected nitro at V%d (%d/%d charges)", racer.name, racer.currentVertex,
                racer.nitroCharges, Rules::kMaxNitroCharges);
        }

        // Fuel: at most one purchase per visit (refuelledAtVertex), and the
        // manual human only ever buys through CmdRefuel.
        bool atStation = (v.type == model::VertexType::FuelStation && v.active);
        bool alreadyBoughtHere = (racer.refuelledAtVertex == racer.currentVertex);
        if (!atStation || alreadyBoughtHere || racer.state != model::RacerState::AtVertex) return;

        if (!racer.isHuman)
        {
            if (NeedsFuel(racer)) BeginRefuel(racer, m_strategies[index]->RefuelAmount(racer));
        }
        else if (m_humanAutopilot)
        {
            int amount = RecommendedRefuelFor(racer);
            if (amount <= 0 && NeedsFuel(racer)) amount = Rules::kMaxStationPurchaseTenths;
            if (amount > 0) BeginRefuel(racer, amount);
        }
    }

    void RaceEngine::BeginRefuel(model::Racer& racer, int amountTenths)
    {
        int capacity = Rules::kMaxFuelTenths - racer.fuelTenths;
        int buy = MathUtil::Min(amountTenths, capacity);
        buy = MathUtil::Min(buy, Rules::kMaxStationPurchaseTenths);
        if (buy <= 0) return;

        racer.fuelTenths += buy;
        racer.refuelledAtVertex = racer.currentVertex;
        racer.state = model::RacerState::Refuelling;
        racer.refuelEndMs = m_nowMs + Rules::kStationStopTimeMs;
        LogLine("%s refuelling at V%d: +%.1f units", racer.name, racer.currentVertex, buy / 10.0);
    }

    bool RaceEngine::NeedsFuel(const model::Racer& racer) const
    {
        // Opponents follow the brief's rule: refuel below 25% of the tank.
        double threshold = Rules::kMaxFuelTenths * Rules::kOpponentLowFuelThreshold;
        if (!racer.isHuman && racer.fuelTenths < threshold) return true;

        // Everyone: the rest of the planned route cannot be driven on the
        // fuel in the tank.
        int start = racer.plannedEdgeCursor;
        int remaining = RouteFuelTenths(*m_graph, racer.plannedEdgePath + start, racer.plannedEdgePathLen - start);
        return remaining > racer.fuelTenths;
    }

    bool RaceEngine::PlanReachesStation(const model::Racer& racer) const
    {
        // A station counts only if the racer can get there on the fuel in
        // the tank: walk the plan edge by edge (edge e leads to vertex e+1)
        // and give up as soon as the running fuel total exceeds the tank.
        // Counting any station "somewhere ahead" let a racer drive past its
        // last chance to detour and run dry short of a station.
        int used = 0;
        for (int e = racer.plannedEdgeCursor; e < racer.plannedEdgePathLen; ++e)
        {
            used += EdgeFuelTenths(*m_graph, racer.plannedEdgePath[e]);
            if (used > racer.fuelTenths) return false;

            const model::Vertex& v = m_graph->GetVertex(racer.plannedVertexPath[e + 1]);
            if (v.type == model::VertexType::FuelStation && v.active) return true;
        }
        return false;
    }

    int RaceEngine::RecommendedRefuelFor(const model::Racer& racer) const
    {
        if (racer.currentVertex < 0) return 0;
        const model::Vertex& v = m_graph->GetVertex(racer.currentVertex);
        if (v.type != model::VertexType::FuelStation || !v.active) return 0;

        int room = MathUtil::Min(Rules::kMaxFuelTenths - racer.fuelTenths, Rules::kMaxStationPurchaseTenths);
        if (room <= 0) return 0;

        // The refuelling DP over the WHOLE rest of the race from here: this
        // station is stop 0 (a purchase is possible), then every station the
        // racer's own route passes on the remaining laps.
        DynArray<int> vertices;
        DynArray<int> edges;
        int lapsAfterThis = m_laps - racer.lap;
        if (!BuildRaceRoute(*m_graph, racer.currentVertex, racer.nextCheckpoint - 1, lapsAfterThis,
            m_finishVertex, m_humanWeights, m_costReferences, &vertices, &edges))
        {
            return room;
        }

        algo::RefuelProblem problem;
        BuildRefuelProblem(*m_graph, vertices, edges, racer.fuelTenths, &problem);
        algo::RefuelPlan plan = algo::PlanRefuelling(problem);

        // Infeasible means no plan along this route reaches the finish:
        // take as much as this stop allows and let later detours top up.
        if (!plan.feasible) return room;
        return (plan.purchase.Size() > 0) ? plan.purchase[0] : 0;
    }

    int RaceEngine::RecommendedRefuel() const
    {
        for (int i = 0; i < m_racers.Size(); ++i)
        {
            if (m_racers[i].isHuman) return RecommendedRefuelFor(m_racers[i]);
        }
        return 0;
    }

    void RaceEngine::StorePlan(model::Racer& racer, const DynArray<int>& vertices, const DynArray<int>& edges)
    {
        int vn = MathUtil::Min(vertices.Size(), model::kMaxPlannedPathLen);
        for (int i = 0; i < vn; ++i) racer.plannedVertexPath[i] = vertices[i];
        racer.plannedVertexPathLen = vn;

        int en = MathUtil::Min(edges.Size(), model::kMaxPlannedPathLen);
        for (int i = 0; i < en; ++i) racer.plannedEdgePath[i] = edges[i];
        racer.plannedEdgePathLen = en;
        racer.plannedEdgeCursor = 0;
    }

    bool RaceEngine::IsLegalEdgeFrom(int vertexId, int edgeId) const
    {
        if (edgeId < 0 || edgeId >= m_graph->EdgeCount()) return false;
        for (const model::AdjNode* node = m_graph->Neighbors(vertexId); node != nullptr; node = node->next)
        {
            if (node->edgeId == edgeId) return m_graph->IsTraversable(edgeId);
        }
        return false;
    }

    void RaceEngine::TryStartNextEdge(model::Racer& racer, int index)
    {
        CheckVertexPickups(racer, index);
        if (racer.state != model::RacerState::AtVertex) return;   // e.g. a refuel just started

        int edgeId = -1;
        bool manualHuman = racer.isHuman && !m_humanAutopilot;

        if (manualHuman)
        {
            if (m_pendingEdgeChoice < 0) return;
            edgeId = m_pendingEdgeChoice;
            m_pendingEdgeChoice = -1;
            if (!IsLegalEdgeFrom(racer.currentVertex, edgeId))
            {
                String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError),
                    "chosen edge is not a legal move from here");
                return;
            }
        }
        else
        {
            // Short of fuel with no station ahead on the current plan:
            // discard it so ReplanRoute can route through a station.
            bool hasPlan = racer.plannedEdgeCursor < racer.plannedEdgePathLen;
            if (hasPlan && NeedsFuel(racer) && !PlanReachesStation(racer))
            {
                racer.plannedEdgeCursor = racer.plannedEdgePathLen;
            }

            if (racer.plannedEdgeCursor >= racer.plannedEdgePathLen) ReplanRoute(racer, index);

            if (racer.plannedEdgeCursor >= racer.plannedEdgePathLen)
            {
                EnterWaiting(racer, "no route to next checkpoint");
                return;
            }

            edgeId = racer.plannedEdgePath[racer.plannedEdgeCursor];
            if (!m_graph->IsTraversable(edgeId))
            {
                racer.plannedEdgeCursor = racer.plannedEdgePathLen;
                ReplanRoute(racer, index);
                if (racer.plannedEdgeCursor >= racer.plannedEdgePathLen)
                {
                    EnterWaiting(racer, "planned edge became blocked, no alternative");
                    return;
                }
                edgeId = racer.plannedEdgePath[racer.plannedEdgeCursor];
            }
        }

        const model::Edge& edge = m_graph->GetEdge(edgeId);
        int target = (edge.from == racer.currentVertex) ? edge.to : edge.from;

        int need = EdgeFuelTenthsNeeded(edgeId);
        if (racer.fuelTenths < need)
        {
            EnterWaiting(racer, "insufficient fuel for next edge");
            return;
        }

        racer.currentEdge = edgeId;
        racer.currentTargetVertex = target;
        racer.edgeProgress = 0.0;
        racer.currentVertex = -1;
        racer.refuelledAtVertex = -1;   // leaving: the next station visit may buy again
        racer.state = model::RacerState::OnEdge;

        if (!manualHuman) ++racer.plannedEdgeCursor;
    }

    void RaceEngine::ReplanRoute(model::Racer& racer, int index)
    {
        racer.plannedVertexPathLen = 0;
        racer.plannedEdgePathLen = 0;
        racer.plannedEdgeCursor = 0;

        if (racer.isHuman)
        {
            // The human's autopilot routes with A* (Opponent 3 and the
            // resource planner use Dijkstra): same optimal cost -- the
            // heuristic is consistent, see algo/AStar.h -- with fewer
            // vertices expanded per leg. This is A*'s in-race role.
            algo::PlanResult plan = algo::PlanLap(*m_graph, racer.currentVertex, racer.nextCheckpoint - 1,
                m_finishVertex, m_humanWeights, m_costReferences, algo::RouteAlgo::AStar);
            if (plan.found) StorePlan(racer, plan.vertexPath, plan.edgePath);
        }
        else
        {
            StrategyContext context;
            context.mainRoute = &m_mainRoute;
            context.detours = &m_detours;
            context.shortcutPreference = &m_shortcutPreference;
            context.finishVertex = m_finishVertex;
            context.costReferences = m_costReferences;

            StrategyPlan plan = m_strategies[index]->PlanRoute(*m_graph, racer, context);
            if (plan.found) StorePlan(racer, plan.vertexPath, plan.edgePath);
        }

        // Refuelling (Act 3 in gameplay, the brief's "Refuelling" column of
        // section 4.4): if this plan cannot be driven on the fuel in the
        // tank and passes no station, route through one first. Opponents 1
        // and 2 take the first eligible station along their own route,
        // Opponent 3 and the human autopilot the nearest reachable one. The
        // station leg and the rest of the lap after it are planned with the
        // human's preset (human) or by distance (opponents).
        if (racer.plannedEdgePathLen == 0 || !NeedsFuel(racer) || PlanReachesStation(racer)) return;

        bool firstOnRoute = (racer.strategyType == model::StrategyType::FixedRoute
            || racer.strategyType == model::StrategyType::ShortcutLover);
        StationChoice choice = firstOnRoute ? StationChoice::FirstOnRoute : StationChoice::Nearest;
        model::Weights weights = racer.isHuman ? m_humanWeights : model::WeightsForPreset(model::Preset::Distance);

        DynArray<int> vertices;
        DynArray<int> edges;
        int station = -1;
        if (PlanStationDetour(*m_graph, racer.currentVertex, racer.fuelTenths, racer.nextCheckpoint - 1,
            m_finishVertex, weights, m_costReferences, choice, racer.plannedVertexPath,
            racer.plannedVertexPathLen, &vertices, &edges, &station))
        {
            StorePlan(racer, vertices, edges);
            LogLine("%s low on fuel (%.1f units): heading to station V%d", racer.name,
                racer.fuelTenths / 10.0, station);
        }
    }

    void RaceEngine::EnterWaiting(model::Racer& racer, const char* reason)
    {
        if (racer.state != model::RacerState::Waiting)
        {
            racer.waitingStartMs = m_nowMs;
            racer.state = model::RacerState::Waiting;
            LogLine("%s waiting: %s", racer.name, reason);
        }
    }

    void RaceEngine::ApplyRecoveryCheck(model::Racer& racer, int index)
    {
        bool manualHuman = racer.isHuman && !m_humanAutopilot;
        bool shouldRetry = manualHuman ? (m_pendingEdgeChoice >= 0) : m_needsReplan[index];

        if (shouldRetry)
        {
            // A re-plan that still fails for the SAME underlying reason
            // (e.g. fuel, which no graph event can fix) must not reset the
            // clock: save the original wait-start time and restore it if
            // the retry leaves the racer Waiting again. Without this, a
            // racer stuck for a reason unrelated to the graph could flip
            // Waiting -> AtVertex -> Waiting on every retry, each time
            // resetting waitingStartMs to "now", and the 60s DNF timeout
            // would never actually elapse.
            int savedWaitingStart = racer.waitingStartMs;

            if (!manualHuman)
            {
                ReplanRoute(racer, index);
                m_needsReplan[index] = false;
            }

            racer.state = model::RacerState::AtVertex;
            TryStartNextEdge(racer, index);

            if (racer.state == model::RacerState::Waiting)
            {
                racer.waitingStartMs = savedWaitingStart;
            }
            else if (racer.state != model::RacerState::AtVertex)
            {
                return;   // moving again (OnEdge/Refuelling)
            }
        }

        if (racer.state == model::RacerState::Waiting
            && m_nowMs - racer.waitingStartMs >= Rules::kRecoveryWaitTimeoutMs)
        {
            racer.state = model::RacerState::Dnf;
            LogLine("%s DNF: stuck for %dms with no valid route", racer.name, Rules::kRecoveryWaitTimeoutMs);
        }
    }

    void RaceEngine::InvalidateStalePlans()
    {
        for (int i = 0; i < m_racers.Size(); ++i)
        {
            model::Racer& racer = m_racers[i];
            if (racer.isHuman && !m_humanAutopilot) continue;

            bool stale = false;
            for (int k = racer.plannedEdgeCursor; k < racer.plannedEdgePathLen; ++k)
            {
                if (!m_graph->IsTraversable(racer.plannedEdgePath[k]))
                {
                    stale = true;
                    break;
                }
            }
            if (stale)
            {
                racer.plannedEdgeCursor = racer.plannedEdgePathLen;
                m_needsReplan[i] = true;
            }
        }
    }

    void RaceEngine::CheckRaceOverConditions()
    {
        bool allSettled = true;
        for (int i = 0; i < m_racers.Size(); ++i)
        {
            model::RacerState s = m_racers[i].state;
            if (s != model::RacerState::Finished && s != model::RacerState::Dnf) allSettled = false;
        }
        if (allSettled)
        {
            m_raceOver = true;
            return;
        }

        if (m_leaderFinishMs >= 0 && m_nowMs >= m_leaderFinishMs + Rules::kRaceFinishGraceMs)
        {
            m_raceOver = true;
            LogLine("Race ended by time limit at t=%dms", m_nowMs);
            return;
        }

        if (m_nowMs >= Rules::kRaceHardCapMs)
        {
            m_raceOver = true;
            LogLine("Race ended by hard cap at t=%dms", m_nowMs);
        }
    }

    model::Racer* RaceEngine::FindHuman()
    {
        for (int i = 0; i < m_racers.Size(); ++i)
        {
            if (m_racers[i].isHuman) return &m_racers[i];
        }
        return nullptr;
    }

    bool RaceEngine::CmdChooseEdge(int edgeId)
    {
        model::Racer* racer = FindHuman();
        if (racer == nullptr)
        {
            String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError), "no human racer");
            return false;
        }
        if (m_humanAutopilot)
        {
            String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError), "autopilot is on");
            return false;
        }
        if (racer->state != model::RacerState::AtVertex && racer->state != model::RacerState::Waiting)
        {
            String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError), "not stopped at a vertex");
            return false;
        }
        if (!IsLegalEdgeFrom(racer->currentVertex, edgeId))
        {
            String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError), "illegal edge from here");
            return false;
        }

        m_pendingEdgeChoice = edgeId;
        return true;
    }

    bool RaceEngine::CmdActivateNitro()
    {
        model::Racer* racer = FindHuman();
        if (racer == nullptr)
        {
            String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError), "no human racer");
            return false;
        }
        if (racer->nitroCharges <= 0)
        {
            String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError), "no nitro charges");
            return false;
        }
        if (racer->nitroActive)
        {
            String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError), "nitro already active");
            return false;
        }
        if (racer->state != model::RacerState::OnEdge && racer->state != model::RacerState::AtVertex)
        {
            String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError), "cannot activate nitro right now");
            return false;
        }

        --racer->nitroCharges;
        racer->nitroActive = true;
        racer->nitroTimeLeftMs = Rules::kNitroDurationMs;
        return true;
    }

    bool RaceEngine::CmdRefuel(int tenths)
    {
        model::Racer* racer = FindHuman();
        if (racer == nullptr)
        {
            String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError), "no human racer");
            return false;
        }
        if (racer->state != model::RacerState::AtVertex)
        {
            String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError), "not stopped at a vertex");
            return false;
        }
        const model::Vertex& v = m_graph->GetVertex(racer->currentVertex);
        if (v.type != model::VertexType::FuelStation || !v.active)
        {
            String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError), "not at an active fuel station");
            return false;
        }
        if (tenths < 0 || tenths > Rules::kMaxStationPurchaseTenths)
        {
            String::StrCopy(m_lastCommandError, sizeof(m_lastCommandError), "purchase amount out of range");
            return false;
        }

        if (tenths > 0) BeginRefuel(*racer, tenths);
        return true;
    }

    bool RaceEngine::CmdSetAutopilot(bool on)
    {
        m_humanAutopilot = on;
        model::Racer* racer = FindHuman();
        if (racer != nullptr) racer->plannedEdgeCursor = racer->plannedEdgePathLen;
        return true;
    }

    bool RaceEngine::CmdSetPreset(model::Preset preset)
    {
        m_humanPreset = preset;
        m_humanWeights = model::WeightsForPreset(preset);
        model::Racer* racer = FindHuman();
        if (racer != nullptr) racer->plannedEdgeCursor = racer->plannedEdgePathLen;
        return true;
    }

    void RaceEngine::GetResults(DynArray<model::RaceResultRow>* outResults) const
    {
        outResults->Clear();

        int n = m_racers.Size();

        // Finish order: Merge Sort (stable, the primary ranking method) with
        // the shared finish comparator, over pointers -- a Racer carries its
        // whole planned path, so copying them to sort would be wasteful.
        DynArray<const model::Racer*> order;
        for (int i = 0; i < n; ++i) order.Push(&m_racers[i]);
        algo::MergeSort<const model::Racer*, algo::FinishOrderPtr>(order.Data(), order.Size());

        for (int rank = 0; rank < n; ++rank)
        {
            const model::Racer& racer = *order[rank];
            model::RaceResultRow row;
            row.racerId = racer.id;
            row.finishPosition = rank + 1;
            row.finishTimeMs = (racer.state == model::RacerState::Finished) ? racer.finishTimeMs : -1;
            row.bestLapTimeMs = racer.bestLapTimeMs;
            row.bonusPoints = 0;
            row.pointsAwarded = 0;
            row.status = (racer.state == model::RacerState::Finished) ? model::RaceStatus::Finished
                : (racer.state == model::RacerState::Dnf) ? model::RaceStatus::Dnf : model::RaceStatus::TimeLimit;
            row.lapsCompleted = (racer.lap - 1) + (row.status == model::RaceStatus::Finished ? 1 : 0);
            row.checkpointsCompletedInFinalLap = racer.checkpointsCompletedThisLap;
            outResults->Push(row);
        }

        int fastestIdx = -1;
        for (int i = 0; i < outResults->Size(); ++i)
        {
            if ((*outResults)[i].status != model::RaceStatus::Finished) continue;
            if (fastestIdx == -1 || (*outResults)[i].bestLapTimeMs < (*outResults)[fastestIdx].bestLapTimeMs)
            {
                fastestIdx = i;
            }
        }
        if (fastestIdx != -1) (*outResults)[fastestIdx].bonusPoints = Rules::kFastestLapBonusPoints;

        for (int i = 0; i < outResults->Size(); ++i)
        {
            model::RaceResultRow& row = (*outResults)[i];
            int pos = row.finishPosition;
            int points = (row.status == model::RaceStatus::Finished && pos >= 1 && pos <= Rules::kChampionshipPointsCount)
                ? Rules::kChampionshipPoints[pos - 1] : 0;
            row.pointsAwarded = points + row.bonusPoints;
        }
    }

    unsigned long long RaceEngine::DeterminismHash() const
    {
        DynArray<model::RaceResultRow> results;
        GetResults(&results);

        unsigned long long hash = 1469598103934665603ULL;
        for (int i = 0; i < results.Size(); ++i)
        {
            const model::RaceResultRow& r = results[i];
            unsigned long long values[5] =
            {
                static_cast<unsigned long long>(r.racerId),
                static_cast<unsigned long long>(r.finishPosition),
                static_cast<unsigned long long>(r.finishTimeMs),
                static_cast<unsigned long long>(r.bestLapTimeMs),
                static_cast<unsigned long long>(r.status),
            };
            for (int k = 0; k < 5; ++k)
            {
                hash ^= values[k];
                hash *= 1099511628211ULL;
            }
        }
        return hash;
    }
}
