#include "EffectManager.h"
#include "../core/util/MathUtil.h"

namespace sim
{
    EffectManager::EffectManager(model::Graph* graph)
        : m_graph(graph), m_nextEffectId(0)
    {
        int edgeCount = graph->EdgeCount();
        m_edgeEffects.Resize(edgeCount);
        m_manuallyBlocked.Resize(edgeCount);

        for (int i = 0; i < edgeCount; ++i)
        {
            m_edgeEffects[i] = new LinkedList<ActiveEffect>();
            m_manuallyBlocked[i] = (graph->GetEdge(i).baseStatus == model::EdgeStatus::Blocked);
            RecomputeEdge(i);
        }
    }

    EffectManager::~EffectManager()
    {
        for (int i = 0; i < m_edgeEffects.Size(); ++i) delete m_edgeEffects[i];
    }

    int EffectManager::AddEdgeEffect(int edgeId, EffectType type, double magnitude, int expiresAtMs)
    {
        ActiveEffect effect;
        effect.effectId = m_nextEffectId;
        ++m_nextEffectId;
        effect.type = type;
        effect.magnitude = magnitude;
        effect.expiresAtMs = expiresAtMs;

        m_edgeEffects[edgeId]->PushFront(effect);
        RecomputeEdge(edgeId);
        return effect.effectId;
    }

    void EffectManager::RemoveEdgeEffect(int edgeId, int effectId)
    {
        m_edgeEffects[edgeId]->RemoveIf([effectId](const ActiveEffect& e)
        {
            return e.effectId == effectId;
        });
        RecomputeEdge(edgeId);
    }

    void EffectManager::SetEdgeBlocked(int edgeId, bool blocked)
    {
        m_manuallyBlocked[edgeId] = blocked;
        RecomputeEdge(edgeId);
    }

    void EffectManager::SetShortcutOpen(int edgeId, bool open)
    {
        SetEdgeBlocked(edgeId, !open);
    }

    void EffectManager::SetVertexActive(int vertexId, bool active)
    {
        m_graph->SetVertexActive(vertexId, active);
    }

    void EffectManager::RecomputeEdge(int edgeId)
    {
        model::Edge& e = m_graph->GetEdgeMutable(edgeId);

        double maxOilBonus = 0.0;
        double minSpeedCapFraction = -1.0;

        for (const LinkedListNode<ActiveEffect>* node = m_edgeEffects[edgeId]->Head();
             node != nullptr; node = node->next)
        {
            const ActiveEffect& effect = node->value;
            if (effect.type == EffectType::OilSpill)
            {
                if (effect.magnitude > maxOilBonus) maxOilBonus = effect.magnitude;
            }
            else if (effect.type == EffectType::SpeedRestriction)
            {
                if (minSpeedCapFraction < 0.0 || effect.magnitude < minSpeedCapFraction)
                {
                    minSpeedCapFraction = effect.magnitude;
                }
            }
        }

        int effDifficulty = e.difficulty + static_cast<int>(maxOilBonus);
        e.effDifficulty = MathUtil::Min(effDifficulty, 5);

        double baseSpeed = (e.baseTime > 0.0) ? (e.distance / e.baseTime) : 0.0;
        e.speedCap = (minSpeedCapFraction < 0.0) ? -1.0 : (baseSpeed * minSpeedCapFraction);

        e.obstaclePenalty = (maxOilBonus > 0.0) ? 0.5 : 0.0;

        if (m_manuallyBlocked[edgeId])
        {
            e.status = model::EdgeStatus::Blocked;
        }
        else if (maxOilBonus > 0.0 || minSpeedCapFraction >= 0.0)
        {
            e.status = model::EdgeStatus::Restricted;
        }
        else
        {
            e.status = model::EdgeStatus::Open;
        }
    }
}
