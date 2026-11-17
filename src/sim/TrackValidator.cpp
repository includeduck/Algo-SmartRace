#include "TrackValidator.h"
#include "ResourcePlanner.h"
#include "../algo/Bfs.h"
#include "../algo/Dfs.h"
#include "../algo/Dijkstra.h"
#include "../algo/RefuelDp.h"
#include "../model/CostModel.h"
#include "../model/Rules.h"
#include "../core/util/MathUtil.h"
#include "../core/util/String.h"

#include <cstdio>

namespace sim
{
    namespace
    {
        // Straight-line distance may exceed the stored distance by file
        // rounding only (.trk coordinates are written to 0.1, distances to
        // 0.01): allow that much, never more.
        const double kDistanceTolerance = 0.25;

        void Fail(ValidationReport* report, ValidationCheck check, const char* message)
        {
            int i = static_cast<int>(check);
            report->check[i] = false;
            String::StrCopy(report->detail[i], static_cast<int>(sizeof(report->detail[i])), message);
        }

        bool Reachable(const model::Graph& graph, int from, int to)
        {
            if (from == to) return true;
            return algo::Bfs(graph, from, to).found;
        }

        bool FuelPlanExists(const model::Graph& graph, int start, int finish, int laps)
        {
            model::Weights weights = model::WeightsForPreset(model::Preset::Balanced);
            model::CostReferences refs = model::ComputeReferences(graph);

            DynArray<int> vertices;
            DynArray<int> edges;
            algo::RefuelProblem problem;

            if (BuildRaceRoute(graph, start, 0, laps - 1, finish, weights, refs, &vertices, &edges))
            {
                BuildRefuelProblem(graph, vertices, edges, Rules::kMaxFuelTenths, &problem);
                if (algo::PlanRefuelling(problem).feasible) return true;
            }

            for (int s = 0; s < graph.VertexCount(); ++s)
            {
                const model::Vertex& v = graph.GetVertex(s);
                if (v.type != model::VertexType::FuelStation || !v.active) continue;

                vertices.Clear();
                edges.Clear();
                bool ok = AppendLapVia(graph, start, 0, s, finish, weights, refs, &vertices, &edges);
                for (int lap = 1; lap < laps && ok; ++lap)
                {
                    ok = AppendLapVia(graph, finish, 0, s, finish, weights, refs, &vertices, &edges);
                }
                if (!ok) continue;

                BuildRefuelProblem(graph, vertices, edges, Rules::kMaxFuelTenths, &problem);
                if (algo::PlanRefuelling(problem).feasible) return true;
            }
            return false;
        }
    }

    const char* ValidationReport::FirstFailure() const
    {
        return m_firstFailure;
    }

    const char* ValidationCheckName(ValidationCheck check)
    {
        switch (check)
        {
            case ValidationCheck::HasStartAndFinish: return "Start and Finish";
            case ValidationCheck::CheckpointsComplete: return "Checkpoints 1..10";
            case ValidationCheck::DistancesAdmissible: return "Distances >= straight line";
            case ValidationCheck::Connected: return "Connected";
            case ValidationCheck::CheckpointsReachableInOrder: return "Checkpoints reachable in order";
            case ValidationCheck::LapReturns: return "Lap returns to CP1";
            case ValidationCheck::HasCycle: return "Has a cycle";
            case ValidationCheck::HasDeadEnd: return "Has a dead end";
            case ValidationCheck::EnoughShortcuts: return "At least 2 shortcuts";
            case ValidationCheck::HasAlternativeRoute: return "Alternative Start-Finish route";
            case ValidationCheck::FuelPlanFeasible: return "Feasible fuel plan";
            case ValidationCheck::Count:
            default: return "?";
        }
    }

    ValidationReport ValidateTrack(const model::Graph& graph, int laps)
    {
        ValidationReport report;
        for (int i = 0; i < kValidationCheckCount; ++i)
        {
            report.check[i] = true;
            report.detail[i][0] = '\0';
        }
        report.m_firstFailure[0] = '\0';

        int n = graph.VertexCount();
        char buf[112];

        // Start / Finish.
        int start = -1;
        int finish = -1;
        int startCount = 0;
        int finishCount = 0;
        for (int v = 0; v < n; ++v)
        {
            model::VertexType t = graph.GetVertex(v).type;
            if (t == model::VertexType::Start)
            {
                start = v;
                ++startCount;
            }
            if (t == model::VertexType::Finish)
            {
                finish = v;
                ++finishCount;
            }
        }
        if (startCount != 1 || finishCount != 1)
        {
            snprintf(buf, sizeof(buf), "found %d Start and %d Finish vertices; need exactly one each", startCount, finishCount);
            Fail(&report, ValidationCheck::HasStartAndFinish, buf);
        }

        // Checkpoints: orders 1..K, each exactly once, K >= the minimum.
        int maxOrder = 0;
        for (int v = 0; v < n; ++v)
        {
            int order = graph.GetVertex(v).checkpointOrder;
            if (order > maxOrder) maxOrder = order;
        }
        DynArray<int> checkpointAt;   // order -> vertex
        checkpointAt.Resize(maxOrder + 1);
        for (int i = 0; i <= maxOrder; ++i) checkpointAt[i] = -1;
        bool checkpointsOk = (maxOrder >= Rules::kMandatoryCheckpointsPerCircuit);
        for (int v = 0; v < n && checkpointsOk; ++v)
        {
            int order = graph.GetVertex(v).checkpointOrder;
            if (order < 0) continue;
            if (order == 0 || checkpointAt[order] != -1)
            {
                checkpointsOk = false;
            }
            else
            {
                checkpointAt[order] = v;
            }
        }
        for (int order = 1; order <= maxOrder && checkpointsOk; ++order)
        {
            if (checkpointAt[order] == -1) checkpointsOk = false;
        }
        if (!checkpointsOk)
        {
            snprintf(buf, sizeof(buf), "checkpoint orders must be exactly 1..K with K >= %d (highest found: %d)",
                Rules::kMandatoryCheckpointsPerCircuit, maxOrder);
            Fail(&report, ValidationCheck::CheckpointsComplete, buf);
        }

        // Distances never shorter than the straight line.
        for (int e = 0; e < graph.EdgeCount(); ++e)
        {
            const model::Edge& edge = graph.GetEdge(e);
            const model::Vertex& a = graph.GetVertex(edge.from);
            const model::Vertex& b = graph.GetVertex(edge.to);
            double straight = MathUtil::Dist2D(a.x, a.y, b.x, b.y);
            if (edge.distance + kDistanceTolerance < straight)
            {
                snprintf(buf, sizeof(buf), "road %d is %.2f long but its ends are %.2f apart", e, edge.distance, straight);
                Fail(&report, ValidationCheck::DistancesAdmissible, buf);
                break;
            }
        }

        algo::DfsComponentsResult components = algo::DfsComponents(graph);
        if (components.componentCount != 1)
        {
            snprintf(buf, sizeof(buf), "the roads form %d separate pieces", components.componentCount);
            Fail(&report, ValidationCheck::Connected, buf);
        }

        bool endpointsKnown = (start >= 0 && finish >= 0 && checkpointsOk);

        if (endpointsKnown)
        {
            int previous = start;
            for (int order = 1; order <= maxOrder + 1; ++order)
            {
                int next = (order <= maxOrder) ? checkpointAt[order] : finish;
                if (!Reachable(graph, previous, next))
                {
                    if (order <= maxOrder)
                    {
                        snprintf(buf, sizeof(buf), "CP%d cannot be reached after the previous stop", order);
                    }
                    else
                    {
                        snprintf(buf, sizeof(buf), "the Finish cannot be reached after CP%d", maxOrder);
                    }
                    Fail(&report, ValidationCheck::CheckpointsReachableInOrder, buf);
                    break;
                }
                previous = next;
            }

            if (!Reachable(graph, finish, checkpointAt[1]))
            {
                Fail(&report, ValidationCheck::LapReturns, "CP1 cannot be reached from the Finish, so there is no second lap");
            }
        }
        else
        {
            Fail(&report, ValidationCheck::CheckpointsReachableInOrder, "skipped: Start, Finish or checkpoints are missing");
            Fail(&report, ValidationCheck::LapReturns, "skipped: Start, Finish or checkpoints are missing");
        }

        if (!algo::DfsCycles(graph).hasCycle)
        {
            Fail(&report, ValidationCheck::HasCycle, "the roads form a tree: no loop, so no alternative paths");
        }

        if (finish < 0 || algo::DfsDeadEnds(graph, finish).deadEndVertices.Size() == 0)
        {
            Fail(&report, ValidationCheck::HasDeadEnd, "no non-mandatory vertex is a dead end");
        }

        int shortcuts = 0;
        for (int e = 0; e < graph.EdgeCount(); ++e)
        {
            if (graph.GetEdge(e).isShortcut) ++shortcuts;
        }
        if (shortcuts < Rules::kMinShortcutsPerCircuit)
        {
            snprintf(buf, sizeof(buf), "%d shortcut(s); at least %d are required", shortcuts, Rules::kMinShortcutsPerCircuit);
            Fail(&report, ValidationCheck::EnoughShortcuts, buf);
        }

        if (start >= 0 && finish >= 0)
        {
            model::Weights distance = model::WeightsForPreset(model::Preset::Distance);
            model::CostReferences refs = model::ComputeReferences(graph);
            algo::PathResult main = algo::Dijkstra(graph, start, finish, distance, refs);
            bool hasAlternative = false;
            if (main.found)
            {
                algo::AlternativeRoutesResult alternatives = algo::DfsAlternativeRoutes(graph, start, finish,
                    main.vertexPath, 1, Rules::kAlternativeRouteMaxDepthMultiplier * n);
                hasAlternative = alternatives.routes.Size() > 0;
            }
            if (!hasAlternative)
            {
                Fail(&report, ValidationCheck::HasAlternativeRoute, "only one route connects Start and Finish");
            }

            if (!FuelPlanExists(graph, start, finish, laps))
            {
                snprintf(buf, sizeof(buf), "no refuelling plan finishes %d laps, even via a fuel station each lap", laps);
                Fail(&report, ValidationCheck::FuelPlanFeasible, buf);
            }
        }
        else
        {
            Fail(&report, ValidationCheck::HasAlternativeRoute, "skipped: no Start/Finish");
            Fail(&report, ValidationCheck::FuelPlanFeasible, "skipped: no Start/Finish");
        }

        report.passed = true;
        for (int i = 0; i < kValidationCheckCount; ++i)
        {
            if (report.check[i]) continue;
            if (report.passed)
            {
                snprintf(report.m_firstFailure, sizeof(report.m_firstFailure), "%s: %s",
                    ValidationCheckName(static_cast<ValidationCheck>(i)), report.detail[i]);
            }
            report.passed = false;
        }
        return report;
    }
}
