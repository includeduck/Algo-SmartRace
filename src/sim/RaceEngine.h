#pragma once

#include "../model/Graph.h"
#include "../model/Records.h"
#include "../model/CostModel.h"
#include "../model/Rules.h"
#include "../io/TrackLoader.h"
#include "EventQueue.h"
#include "EffectManager.h"
#include "Strategies.h"
#include "../core/util/Log.h"
#include "../core/ds/DynArray.h"

namespace sim
{
    struct RaceConfig
    {
        RaceConfig()
            : laps(Rules::kDefaultLaps), opponentCount(3), humanPreset(model::Preset::Balanced),
              eventSeed(0), humanAutopilot(true), startNitroCharges(Rules::kDefaultStartNitroCharges)
        {
        }

        int laps;
        int opponentCount;          // not counting the human; 3-9 per the brief's minimum/table cap
        model::Preset humanPreset;
        unsigned long long eventSeed;
        bool humanAutopilot;
        int startNitroCharges;      // every racer's initial allocation (brief 4.2 rule 1), 0..kMaxNitroCharges
    };

    // RaceEngine: the deterministic race simulation (Project.pdf sections
    // 4.1-4.5). Fixed 20ms step, integer clock -- results never depend on
    // frame rate or wall-clock time (Step() is the only way time advances).
    //
    // Does not own `graph`: the caller (API layer) keeps the Graph alive
    // for as long as a loaded circuit is current, and the engine mutates
    // its effective edge/vertex state in place through its own
    // EffectManager, so any other code holding the same Graph pointer
    // (e.g. the API's cached GraphView) sees live updates.
    class RaceEngine : public IEventSink
    {
    public:
        RaceEngine();
        ~RaceEngine();
        RaceEngine(const RaceEngine&) = delete;
        RaceEngine& operator=(const RaceEngine&) = delete;

        // `graph` is borrowed (not owned). `trackData` supplies the
        // MAINROUTE, DETOUR, SHORTCUT and EVENT lines; the engine copies
        // what it needs out of it, so `trackData` need not outlive this
        // call.
        void Start(model::Graph* graph, const io::TrackData& trackData, const RaceConfig& config);

        void Step();   // advances exactly Rules::kSimStepMs of simulated time
        bool IsRaceOver() const
        {
            return m_raceOver;
        }

        int NowMs() const
        {
            return m_nowMs;
        }

        int RacerCount() const
        {
            return m_racers.Size();
        }

        const model::Racer& GetRacer(int index) const
        {
            return m_racers[index];
        }

        // Human commands. Each validates and returns false (with
        // LastCommandError() set) rather than ever doing something illegal.
        bool CmdChooseEdge(int edgeId);
        bool CmdActivateNitro();
        bool CmdRefuel(int tenths);
        bool CmdSetAutopilot(bool on);
        bool CmdSetPreset(model::Preset preset);

        const char* LastCommandError() const
        {
            return m_lastCommandError;
        }

        // How many fuel tenths the refuelling DP recommends the human buy
        // at the station they are stopped at (0 if not at an active
        // station, or if the rest of the race needs no purchase here).
        int RecommendedRefuel() const;

        const Log& EventLog() const
        {
            return m_log;
        }

        // Fills `outResults` (one row per racer) with the standings as of
        // right now -- meaningful as FINAL results once IsRaceOver(), but
        // safe to call mid-race too (e.g. for a live "current order" view).
        void GetResults(DynArray<model::RaceResultRow>* outResults) const;

        // FNV-1a over every racer's final result fields, in racer-id order.
        // Two races started with identical graph/trackData/config and
        // stepped for the same simulated duration must produce the same
        // hash (Project.pdf section 4.4's determinism requirement).
        unsigned long long DeterminismHash() const;

    private:
        void OnEvent(const Event& event) override;

        void StepRacer(int index);
        void AdvanceOnEdge(model::Racer& racer, int index);
        void ArriveAtVertex(model::Racer& racer, int index);
        void CheckVertexPickups(model::Racer& racer, int index);
        void TryStartNextEdge(model::Racer& racer, int index);
        void ReplanRoute(model::Racer& racer, int index);
        void ApplyRecoveryCheck(model::Racer& racer, int index);
        void EnterWaiting(model::Racer& racer, const char* reason);
        void BeginRefuel(model::Racer& racer, int amountTenths);
        void StorePlan(model::Racer& racer, const DynArray<int>& vertices, const DynArray<int>& edges);

        // Short of fuel: an opponent below 25% of the tank, or anyone whose
        // remaining planned route needs more fuel than is in the tank.
        bool NeedsFuel(const model::Racer& racer) const;
        bool PlanReachesStation(const model::Racer& racer) const;
        int RecommendedRefuelFor(const model::Racer& racer) const;
        void InvalidateStalePlans();
        void CheckRaceOverConditions();

        bool IsLegalEdgeFrom(int vertexId, int edgeId) const;
        double EdgeTravelSpeed(int edgeId, bool nitroActive) const;
        int EdgeFuelTenthsNeeded(int edgeId) const;
        model::Racer* FindHuman();

        void LogLine(const char* fmt, ...);

        model::Graph* m_graph;         // not owned
        EffectManager* m_effects;       // owned
        EventQueue* m_events;             // owned; a pointer (not a value member) because Start()
                                           // replaces it wholesale on a restart and EventQueue, like
                                           // most classes here, is neither copyable nor resettable in place
        DynArray<IStrategy*> m_strategies;   // parallel to m_racers, owned; nullptr for the human
        DynArray<model::Racer> m_racers;

        model::CostReferences m_costReferences;
        model::Weights m_humanWeights;
        model::Preset m_humanPreset;
        bool m_humanAutopilot;
        int m_humanIndex;

        int m_laps;
        int m_nowMs;
        bool m_raceOver;
        bool m_graphChangedThisBatch;
        int m_leaderFinishMs;   // -1 until the first racer finishes
        int m_totalCheckpoints;

        Log m_log;
        char m_lastCommandError[160];

        int m_pendingEdgeChoice;                       // human's queued manual choice, -1 if none
        DynArray<int> m_pendingNitroExpireEventId;       // per vertex, -1 if none pending
        DynArray<bool> m_needsReplan;                      // per racer
        int m_startVertex;
        int m_finishVertex;

        DynArray<int> m_mainRoute;                 // owned copy
        DynArray<io::DetourRule*> m_detours;         // owned copies
        DynArray<int> m_shortcutPreference;            // owned copy
    };
}
