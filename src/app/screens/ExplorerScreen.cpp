#include "ExplorerScreen.h"
#include "App.h"
#include "ui/Theme.h"
#include "ui/Format.h"
#include "raygui.h"
#include <cstdio>
#include <cmath>

void ExplorerScreen::Enter(App&)
{
    m_graph = ApiGetGraph();
    m_map.Fit(m_graph);
    m_preset = static_cast<int>(ApiGetSettings().defaultPreset);
    m_source = 0;
    m_target = -1;
    m_scroll = 0;
    m_dirty = true;
    m_selectedVertex = m_selectedEdge = -1;
}

void ExplorerScreen::Refresh()
{
    m_info = ApiGetTrackInfo();
    m_validation = ApiValidateTrack();
    if (m_tab == 1) m_bfs = ApiRunBfs(m_source, m_target);
    if (m_tab == 2)
    {
        m_components = ApiRunDfsComponents();
        m_cycle = ApiRunDfsCycles();
        m_deadEnds = ApiRunDeadEnds();
    }
    if (m_tab == 3)
    {
        m_route = ApiPlanLap(static_cast<Preset>(m_preset), static_cast<RouteAlgo>(m_algorithm));
        m_routeEdges.Copy(m_route.edgePath);
        m_routeVertices.Copy(m_route.vertexPath);
        m_route.edgePath = m_routeEdges.View();
        m_route.vertexPath = m_routeVertices.View();
        RouteView other = ApiPlanLap(static_cast<Preset>(m_preset), m_algorithm == 0 ? RouteAlgo::AStar : RouteAlgo::Dijkstra);
        m_comparisonCost = other.totalCost;
        m_comparisonFound = other.found;
        m_alternatives = ApiCompareRoutes(static_cast<Preset>(m_preset));
    }
    if (m_tab == 4)
    {
        m_pickups = ApiPlanPickups(static_cast<Preset>(m_preset), m_budget);
        m_refuel = ApiPlanRefuel(static_cast<Preset>(m_preset));
    }
    m_dirty = false;
}

void ExplorerScreen::Update(App& app, float)
{
    int action = Ui::TakeAction();
    if (action == 1)
    {
        app.Go(ScreenId::Menu);
        return;
    }
    if (action >= 10 && action < 15)
    {
        m_tab = action - 10;
        m_scroll = 0;
        m_candidate = -1;
        m_dirty = true;
    }
    if (action == 20) m_dirty = true;
    if (action == 21)
    {
        Settings settings = ApiGetSettings();
        settings.defaultPreset = static_cast<Preset>(m_preset);
        if (app.Check(ApiSetSettings(settings))) app.StartRace();
        return;
    }
    if (action == 22)
    {
        app.state.labReturn = ScreenId::Explorer;
        app.Go(ScreenId::Lab);
        return;
    }
    if (action >= 30 && action < 34) m_candidate = action - 30;
    Rectangle c = app.Content();
    float side = c.width < 1000 ? 290.0f : 360.0f;
    m_map.bounds = { c.x, c.y + 100, c.width - side - 20, c.height - 110 };
    m_details = { c.x + c.width - side, c.y + 100, side, c.height - 110 };
    m_graph = ApiGetGraph();
    if (m_dirty) Refresh();
    m_map.Update();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), m_map.bounds))
    {
        m_selectedVertex = m_map.HitVertex(m_graph);
        m_selectedEdge = m_map.HitEdge(m_graph);
        if (m_tab == 1 && m_selectedVertex >= 0)
        {
            if (IsKeyDown(KEY_LEFT_SHIFT)) m_source = m_selectedVertex;
            else m_target = m_selectedVertex;
            m_dirty = true;
        }
    }
    if (CheckCollisionPointRec(GetMousePosition(), m_details)) m_scroll -= GetMouseWheelMove() * 40;
    if (IsKeyPressed(KEY_PAGE_DOWN)) m_scroll += 200;
    if (IsKeyPressed(KEY_PAGE_UP)) m_scroll -= 200;
    float max = m_detailHeight - m_details.height + 30;
    if (max < 0) max = 0;
    if (m_scroll < 0) m_scroll = 0;
    if (m_scroll > max) m_scroll = max;
}

void ExplorerScreen::Details(App&, float x, float& y, float width)
{
    char line[256];
    auto row = [&](const char* text, Color color = Ui::Text)
    {
        Ui::Wrap({ x, y, width, 85 }, text, 17, color);
        int lines = (MeasureText(text, 17) / static_cast<int>(width)) + 1;
        y += lines * 25 + 8;
    };
    if (m_tab == 0)
    {
        row("CIRCUIT OVERVIEW", Ui::Accent);
        snprintf(line, sizeof(line), "%d vertices / %d roads", m_info.vertexCount, m_info.edgeCount);
        row(line);
        snprintf(line, sizeof(line), "%d checkpoints / %d shortcuts", m_info.mandatoryCheckpointCount, m_info.shortcutCount);
        row(line);
        snprintf(line, sizeof(line), "%d race laps (track metadata: %d)", ApiGetSettings().laps, m_info.laps);
        row(line);
        row(m_validation.passed ? "VALIDATION PASSED" : "VALIDATION FAILED", m_validation.passed ? Ui::Accent : Ui::Danger);
        for (int i = 0; i < m_validation.checks.count; ++i)
        {
            const ValidationCheckView& check = m_validation.checks[i];
            snprintf(line, sizeof(line), "%s %s", check.passed ? "+" : "!", check.name);
            row(line, check.passed ? Ui::Muted : Ui::Danger);
            if (!check.passed) row(check.detail, Ui::Danger);
        }
        row("MAP LEGEND", Ui::Accent);
        row("CP: ordered checkpoint. F: fuel. N: nitro. OFF: unavailable.");
        row("X: blocked. !: restricted. Purple roads: shortcuts. Arrows: one way.");
    }
    if (m_tab == 1)
    {
        row("BFS / MINIMUM ROAD COUNT", Ui::Accent);
        Ui::Number({ x + 95, y, width - 95, 34 }, "Source", &m_source, 0, m_graph.vertices.count - 1);
        y += 50;
        Ui::Number({ x + 95, y, width - 95, 34 }, "Target", &m_target, -1, m_graph.vertices.count - 1);
        y += 50;
        row("-1 target: reachability only. Click map: target. Shift-click: source.", Ui::Muted);
        Ui::Button(20, { x, y, width, 36 }, "Run BFS");
        y += 54;
        row(m_target < 0 ? "Reachability query" : (m_bfs.found ? "Destination reachable" : "Destination unreachable"), m_bfs.found || m_target < 0 ? Ui::Accent : Ui::Danger);
        if (m_bfs.found)
        {
            snprintf(line, sizeof(line), "Minimum %d roads (not distance)", m_bfs.path.count - 1);
            row(line);
        }
        row("Reachable checkpoints:");
        for (int i = 0; i < m_bfs.reachableCheckpoints.count; ++i)
        {
            snprintf(line, sizeof(line), "CP%d", m_bfs.reachableCheckpoints[i]);
            row(line, Ui::Gold);
        }
    }
    if (m_tab == 2)
    {
        row("DFS / TRACK STRUCTURE", Ui::Accent);
        snprintf(line, sizeof(line), "%d weakly connected components", m_components.componentCount);
        row(line);
        row(m_cycle.hasCycle ? "Cycle detected" : "No cycle detected", Ui::Gold);
        row("Cycle witness vertices:");
        for (int i = 0; i < m_cycle.witnessCycle.count; ++i)
        {
            snprintf(line, sizeof(line), "V%d", m_cycle.witnessCycle[i]);
            row(line);
        }
        row("Dead-end vertices:");
        if (m_deadEnds.deadEndVertices.count == 0) row("None reported.", Ui::Muted);
        for (int i = 0; i < m_deadEnds.deadEndVertices.count; ++i)
        {
            snprintf(line, sizeof(line), "V%d", m_deadEnds.deadEndVertices[i]);
            row(line, Ui::Danger);
        }
    }
    if (m_tab == 3 || m_tab == 4)
    {
        row(m_tab == 3 ? "CHECKPOINT-AWARE LAP" : "RESOURCE PLANNER", Ui::Accent);
        Ui::Number({ x + 90, y, width - 90, 34 }, "Preset", &m_preset, 0, 3);
        y += 46;
        row(Format::PresetName(static_cast<Preset>(m_preset)), Ui::Gold);
    }
    if (m_tab == 3)
    {
        Ui::Number({ x + 90, y, width - 90, 34 }, "Method", &m_algorithm, 0, 1);
        y += 46;
        row(m_algorithm == 0 ? "Dijkstra" : "A*", Ui::Gold);
        Ui::Button(20, { x, y, width, 36 }, "Compute and compare");
        y += 54;
        if (!m_route.found)
        {
            snprintf(line, sizeof(line), "No complete lap: failed leg %d", m_route.blockedLegIndex);
            row(line, Ui::Danger);
        }
        else
        {
            snprintf(line, sizeof(line), "Optimized scalar cost: %.4f", m_route.totalCost);
            row(line, Ui::Accent);
            snprintf(line, sizeof(line), "Distance %.1f / time %.1fs", m_route.totalDistance, m_route.totalTime);
            row(line);
            snprintf(line, sizeof(line), "Estimated fuel %.1f units", m_route.totalFuel);
            row(line);
            bool equal = m_comparisonFound && fabs(m_comparisonCost - m_route.totalCost) < 0.000001;
            row(equal ? "Dijkstra / A* costs agree" : "Cost comparison requires attention", equal ? Ui::Accent : Ui::Danger);
        }
        row("EXPLORATION ALTERNATIVES", Ui::Gold);
        row("Start-to-Finish candidates may skip checkpoints. Never applied as race plans.", Ui::Muted);
        for (int i = 0; i < m_alternatives.candidates.count; ++i)
        {
            const RouteCandidateView& candidate = m_alternatives.candidates[i];
            snprintf(line, sizeof(line), "%s / %.3f", candidate.label, candidate.totalCost);
            Ui::Button(30 + i, { x, y, width, 34 }, line, true, m_candidate == i);
            y += 44;
            snprintf(line, sizeof(line), "%.1f distance / %.1fs / %.1f fuel", candidate.totalDistance, candidate.totalTime, candidate.totalFuel);
            row(line, Ui::Muted);
        }
        row("All candidates scored under the selected preset. Candidate selection highlights vertices only; exact road IDs are unavailable.", Ui::Muted);
    }
    if (m_tab == 4)
    {
        Ui::Number({ x + 100, y, width - 100, 34 }, "Budget .1s", &m_budget, 0, 2000);
        y += 50;
        Ui::Button(20, { x, y, width, 36 }, "Plan resources");
        y += 54;
        snprintf(line, sizeof(line), "DP %.1fs benefit / %.1fs cost", m_pickups.dpBenefit / 10.0, m_pickups.dpCost / 10.0);
        row(line, Ui::Accent);
        snprintf(line, sizeof(line), "Greedy %.1fs benefit / %.1fs cost", m_pickups.greedyBenefit / 10.0, m_pickups.greedyCost / 10.0);
        row(line, Ui::Gold);
        snprintf(line, sizeof(line), "DP states: %lld", m_pickups.dpStates);
        row(line);
        row("Budget is detour time; fuel and nitro remain distinct resources.", Ui::Muted);
        if (m_pickups.items.count == 0) row("No eligible optional pickups.", Ui::Muted);
        for (int i = 0; i < m_pickups.items.count; ++i)
        {
            const PickupItemView& item = m_pickups.items[i];
            snprintf(line, sizeof(line), "V%d %s: cost %.1fs, benefit %.1fs", item.vertex, item.isNitro ? "nitro" : "fuel", item.costTenthsSec / 10.0, item.benefitTenthsSec / 10.0);
            row(line);
            snprintf(line, sizeof(line), "DP: %s / Greedy: %s", item.chosenByDp ? "take" : "skip", item.chosenByGreedy ? "take" : "skip");
            row(line, Ui::Muted);
        }
        row(m_refuel.feasible ? "REFUEL PLAN FEASIBLE" : "REFUEL PLAN INFEASIBLE", m_refuel.feasible ? Ui::Accent : Ui::Danger);
        snprintf(line, sizeof(line), "%d stops / %.1fs stop time", m_refuel.stopCount, m_refuel.totalStopTimeMs / 1000.0);
        row(line);
        for (int i = 0; i < m_refuel.stops.count; ++i)
        {
            const RefuelStopView& stop = m_refuel.stops[i];
            if (stop.purchaseTenths <= 0) continue;
            snprintf(line, sizeof(line), "V%d: buy %.1f fuel", stop.vertex, stop.purchaseTenths / 10.0);
            row(line, Ui::Gold);
        }
    }
    if (m_selectedVertex >= 0)
    {
        snprintf(line, sizeof(line), "Selected vertex: V%d", m_selectedVertex);
        row(line, Ui::Accent);
    }
    for (int i = 0; i < m_graph.edges.count; ++i)
    {
        const EdgeView& edge = m_graph.edges[i];
        if (edge.id != m_selectedEdge) continue;
        snprintf(line, sizeof(line), "Road %d: V%d -> V%d", edge.id, edge.from, edge.to);
        row(line, Ui::Accent);
        snprintf(line, sizeof(line), "Distance %.1f / base %.1fs / fuel %.1f", edge.distance, edge.baseTime, edge.fuelCost);
        row(line);
        snprintf(line, sizeof(line), "Difficulty %d (effective %d)", edge.difficulty, edge.effDifficulty);
        row(line);
        row("Edge scalar cost is not exposed by the API.", Ui::Muted);
    }
}

void ExplorerScreen::Draw(App& app)
{
    Rectangle c = app.Content();
    Ui::Button(1, { c.x, c.y, 100, 36 }, "Paddock");
    Ui::Label(c.x + 120, c.y + 5, m_info.name ? m_info.name : "No circuit", 24);
    Ui::Button(22, { c.x + c.width - 260, c.y, 110, 36 }, "Lab", app.Available(ScreenId::Lab));
    Ui::Button(21, { c.x + c.width - 140, c.y, 140, 36 }, "Start race", app.Available(ScreenId::Race) && app.state.dataReady && !app.state.raceStarted);
    const char* tabs[] = { "Overview", "BFS", "DFS", "Routes", "Resources" };
    for (int i = 0; i < 5; ++i)
    {
        Ui::Button(10 + i, { c.x + i * (c.width / 5), c.y + 50, c.width / 5 - 8, 34 }, tabs[i], true, m_tab == i);
    }
    ArrayView<int> edges = m_tab == 3 ? m_route.edgePath : ArrayView<int>{};
    ArrayView<int> vertices = m_tab == 1 ? m_bfs.path : (m_tab == 2 ? m_cycle.witnessCycle : ArrayView<int>{});
    if (m_tab == 3 && m_candidate >= 0 && m_candidate < m_alternatives.candidates.count)
    {
        edges = {};
        vertices = m_alternatives.candidates[m_candidate].vertexPath;
    }
    m_map.Draw(m_graph, edges, vertices);
    Ui::Card(m_details);
    Ui::BeginClip(m_details);
    float y = m_details.y + 16 - m_scroll;
    Details(app, m_details.x + 16, y, m_details.width - 32);
    m_detailHeight = y - m_details.y + m_scroll;
    Ui::EndClip();
    if (m_detailHeight > m_details.height) Ui::Label(m_details.x + 12, m_details.y + m_details.height - 19, "Scroll / PgUp / PgDn for more", 14, Ui::Gold);
}
