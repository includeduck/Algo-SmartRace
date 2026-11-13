#include "CostModel.h"
#include "../core/util/MathUtil.h"

namespace model
{
    Weights WeightsForPreset(Preset preset)
    {
        switch (preset)
        {
            case Preset::Speed:
            {
                Weights w = { 0.15, 0.55, 0.05, 0.15, 0.10 };
                return w;
            }
            case Preset::Economy:
            {
                Weights w = { 0.20, 0.10, 0.55, 0.05, 0.10 };
                return w;
            }
            case Preset::Safety:
            {
                Weights w = { 0.10, 0.15, 0.10, 0.35, 0.30 };
                return w;
            }
            case Preset::Distance:
            {
                Weights w = { 1.00, 0.0, 0.0, 0.0, 0.0 };
                return w;
            }
            case Preset::Balanced:
            default:
            {
                Weights w = { 0.25, 0.25, 0.20, 0.15, 0.15 };
                return w;
            }
        }
    }

    CostReferences ComputeReferences(const Graph& graph)
    {
        CostReferences refs;
        refs.distanceRef = 1.0;
        refs.timeRef = 1.0;
        refs.fuelRef = 1.0;

        int n = graph.EdgeCount();
        if (n == 0) return refs;

        double sumDistance = 0.0;
        double sumTime = 0.0;
        double sumFuel = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const Edge& e = graph.GetEdge(i);
            sumDistance += e.distance;
            sumTime += e.baseTime;
            sumFuel += e.fuelCost;
        }

        double count = static_cast<double>(n);
        if (sumDistance > 0.0) refs.distanceRef = sumDistance / count;
        if (sumTime > 0.0) refs.timeRef = sumTime / count;
        if (sumFuel > 0.0) refs.fuelRef = sumFuel / count;

        return refs;
    }

    double EdgeEffectiveTime(const Graph& graph, int edgeId)
    {
        const Edge& e = graph.GetEdge(edgeId);

        double baseSpeed = (e.baseTime > 0.0) ? (e.distance / e.baseTime) : 0.0;
        if (baseSpeed <= 0.0) return e.baseTime;

        double effSpeed = baseSpeed * (1.0 - 0.08 * (e.effDifficulty - e.difficulty));
        if (e.speedCap > 0.0) effSpeed = MathUtil::Min(effSpeed, e.speedCap);
        if (effSpeed <= 0.0) effSpeed = baseSpeed * 0.01;   // never divide by zero or go negative

        return e.distance / effSpeed;
    }

    double EdgeFuelNeed(const Graph& graph, int edgeId)
    {
        const Edge& e = graph.GetEdge(edgeId);
        return e.fuelCost * (1.0 + 0.1 * (e.effDifficulty - 1));
    }

    double EdgeCost(const Graph& graph, int edgeId, const Weights& weights, const CostReferences& refs)
    {
        const Edge& e = graph.GetEdge(edgeId);

        double dTerm = e.distance / refs.distanceRef;
        double tTerm = EdgeEffectiveTime(graph, edgeId) / refs.timeRef;
        double fTerm = EdgeFuelNeed(graph, edgeId) / refs.fuelRef;
        double rTerm = (static_cast<double>(e.effDifficulty) - 1.0) / 4.0;
        double oTerm = e.obstaclePenalty;

        return weights.wd * dTerm + weights.wt * tTerm + weights.wf * fTerm
             + weights.wr * rTerm + weights.wo * oTerm;
    }
}
