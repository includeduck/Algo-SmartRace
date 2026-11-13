#pragma once

#include "Effect.h"
#include "../model/Graph.h"
#include "../core/ds/DynArray.h"
#include "../core/ds/LinkedList.h"

namespace sim
{
    // Owns every active temporary effect (oil spill, speed restriction) on
    // every edge, plus the block/shortcut/service toggles, and recomputes
    // each edge's EFFECTIVE state (Project.pdf section 4.5) in one
    // documented order:
    //   blocked       = manually blocked (RoadBlock/ShortcutToggle-closed)
    //   effDifficulty = min(5, baseDifficulty + MAX active oil bonus)
    //   speedCap      = baseSpeed * MIN active speed-cap fraction (none if no cap)
    //   status        = Blocked, else Restricted if any oil/cap is active, else Open
    // Expiring one effect (RemoveEdgeEffect) only removes that one effectId
    // and recomputes — any other still-active effect on the same edge is
    // untouched, by construction (each is a separate linked-list node).
    //
    // One LinkedList<ActiveEffect> is allocated per edge (addressed by
    // pointer in a DynArray) rather than storing LinkedList BY VALUE in the
    // DynArray — LinkedList has no copy/move support, and DynArray grows by
    // copy/move-assigning elements, so it can only ever hold pointers (or
    // other assignable values), never container values directly. The same
    // pattern is already used by Graph's own adjacency lists.
    class EffectManager
    {
    public:
        explicit EffectManager(model::Graph* graph);
        ~EffectManager();
        EffectManager(const EffectManager&) = delete;
        EffectManager& operator=(const EffectManager&) = delete;

        // Adds a new temporary effect to `edgeId` and recomputes it.
        // Returns the effectId the caller should remember (typically to
        // pass to a scheduled expiry event's RemoveEdgeEffect call).
        int AddEdgeEffect(int edgeId, EffectType type, double magnitude, int expiresAtMs);

        // Removes exactly the one effect with this id (no-op if it is no
        // longer present, e.g. already removed) and recomputes `edgeId`.
        void RemoveEdgeEffect(int edgeId, int effectId);

        void SetEdgeBlocked(int edgeId, bool blocked);     // RoadBlock / RoadClear
        void SetShortcutOpen(int edgeId, bool open);         // ShortcutToggle
        void SetVertexActive(int vertexId, bool active);       // FuelStationToggle / nitro refresh

    private:
        void RecomputeEdge(int edgeId);

        model::Graph* m_graph;   // not owned
        DynArray<LinkedList<ActiveEffect>*> m_edgeEffects;   // one per edge, owned
        DynArray<bool> m_manuallyBlocked;                       // explicit block/shortcut-closed state
        int m_nextEffectId;
    };
}
