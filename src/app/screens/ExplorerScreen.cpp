#include "ExplorerScreen.h"
#include "App.h"
#include "ui/Theme.h"
#include "ui/Format.h"
#include "raygui.h"
#include <cstdio>
#include <cmath>

void ExplorerScreen::Enter(App& app)
{
    m_graph = ApiGetGraph();
    m_map.Fit(m_graph);
    m_live = app.state.raceStarted && !ApiRaceOver();
    m_preset = static_cast<int>(m_live ? app.state.activePreset : ApiGetSettings().defaultPreset);
    m_source = 0;
    m_target = -1;
    m_scroll = 0;
    m_dirty = true;
    m_selectedVertex = m_selectedEdge = -1;
    m_candidate = -1;
    for (int i = 0; i < 7; ++i) m_lastInputs[i] = -99;
    for (int i = 0; i < m_graph.vertices.count; ++i)
    {
        if (m_graph.vertices[i].type == VertexType::Finish) m_routeTarget = m_graph.vertices[i].id;
    }
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
        Preset preset = static_cast<Preset>(m_preset);
        RouteAlgo algorithm = static_cast<RouteAlgo>(m_algorithm);
        m_route = m_routeScope == 0 ? ApiPlanLap(preset, algorithm) : ApiPlanRoute(m_source, m_routeTarget, preset, algorithm);
        m_routeError[0] = '\0';
        if (!m_route.found) snprintf(m_routeError, sizeof(m_routeError), "%s", ApiLastError());
        m_routeEdges.Copy(m_route.edgePath);
        m_routeVertices.Copy(m_route.vertexPath);
        m_route.edgePath = m_routeEdges.View();
        m_route.vertexPath = m_routeVertices.View();
        RouteAlgo otherAlgorithm = m_algorithm == 0 ? RouteAlgo::AStar : RouteAlgo::Dijkstra;
        RouteView other = m_routeScope == 0 ? ApiPlanLap(preset, otherAlgorithm) : ApiPlanRoute(m_source, m_routeTarget, preset, otherAlgorithm);
        m_comparisonCost = other.totalCost;
        m_comparisonFound = other.found;
        m_comparisonExpanded = other.expanded;
        m_alternatives = ApiCompareRoutes(static_cast<Preset>(m_preset));
        m_raceAlternatives = ApiCompareRaceRoutes(static_cast<Preset>(m_preset));
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
    if (action == 23) m_routeScope = 1 - m_routeScope;
    if (action == 24)
    {
        app.Go(ScreenId::Race);
        return;
    }
    if (action >= 30 && action < 34)
    {
        m_candidate = action - 30;
        m_raceCandidate = false;
    }
    if (action >= 40 && action < 44)
    {
        m_candidate = action - 40;
        m_raceCandidate = true;
    }
    if (action == 50 && m_raceCandidate && m_candidate >= 0 && m_candidate < m_raceAlternatives.candidates.count)
    {
        int selected = m_raceAlternatives.candidates[m_candidate].sourcePreset;
        if (selected >= 0 && selected < 4)
        {
            bool applied = false;
            if (m_live)
            {
                applied = app.Check(ApiCmdPreset(static_cast<Preset>(selected)));
                if (applied) app.state.activePreset = static_cast<Preset>(selected);
            }
            else
            {
                Settings settings = ApiGetSettings();
                settings.defaultPreset = static_cast<Preset>(selected);
                applied = app.Check(ApiSetSettings(settings));
            }
            if (applied) m_preset = selected;
        }
    }
    int inputs[] = { m_source, m_target, m_preset, m_algorithm, m_budget, m_routeScope, m_routeTarget };
    for (int i = 0; i < 7; ++i)
    {
        if (inputs[i] != m_lastInputs[i])
        {
            m_dirty = true;
            m_candidate = -1;
        }
        m_lastInputs[i] = inputs[i];
    }
    Rectangle c = app.Content();
    float side = c.width < 1000 ? 290.0f : 360.0f;
    m_map.bounds = { c.x, c.y + 100, c.width - side - 20, c.height - 110 };
    m_details = { c.x + c.width - side, c.y + 100, side, c.height - 110 };
    m_graph = ApiGetGraph();
    if (m_dirty) Refresh();
    m_costs = ApiEdgeCosts(static_cast<Preset>(m_preset));
    m_map.Update();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), m_map.bounds))
    {
        m_selectedVertex = m_map.HitVertex(m_graph);
        m_selectedEdge = m_map.HitEdge(m_graph);
        if ((m_tab == 1 || (m_tab == 3 && m_routeScope == 1)) && m_selectedVertex >= 0)
        {
            if (IsKeyDown(KEY_LEFT_SHIFT)) m_source = m_selectedVertex;
            else if (m_tab == 1) m_target = m_selectedVertex;
            else m_routeTarget = m_selectedVertex;
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
        row(m_tab == 3 ? "ROUTE PLANNER / API v1.1" : "RESOURCE PLANNER", Ui::Accent);
        Ui::Number({ x + 90, y, width - 90, 34 }, "Preset", &m_preset, 0, 3);
        y += 46;
        row(Format::PresetName(static_cast<Preset>(m_preset)), Ui::Gold);
    }
    if (m_tab == 3)
    {
        Ui::Button(23, { x, y, width, 36 }, m_routeScope == 0 ? "Scope: ordered lap" : "Scope: selected endpoints");
        y += 50;
        if (m_routeScope == 1)
        {
            Ui::Number({ x + 90, y, width - 90, 34 }, "Source", &m_source, 0, m_graph.vertices.count - 1);
            y += 46;
            Ui::Number({ x + 90, y, width - 90, 34 }, "Target", &m_routeTarget, 0, m_graph.vertices.count - 1);
            y += 46;
            row("Endpoint search does not force checkpoints. Click target; Shift-click source.", Ui::Muted);
        }
        else if (m_live) row("Lap summary is the Start baseline. Race alternatives below use your live progress.", Ui::Gold);
        Ui::Number({ x + 90, y, width - 90, 34 }, "Method", &m_algorithm, 0, 1);
        y += 46;
        row(m_algorithm == 0 ? "Dijkstra" : "A*", Ui::Gold);
        Ui::Button(20, { x, y, width, 36 }, "Compute and compare");
        y += 54;
        if (!m_route.found)
        {
            snprintf(line, sizeof(line), "No route: failed leg %d", m_route.blockedLegIndex);
            row(line, Ui::Danger);
            row(m_routeError, Ui::Danger);
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
            snprintf(line, sizeof(line), "Expanded: selected %lld / other %lld", m_route.expanded, m_comparisonExpanded);
            row(line);
            snprintf(line, sizeof(line), "Relaxed %lld / heap +%lld -%lld", m_route.relaxed, m_route.heapPushes, m_route.heapPops);
            row(line);
        }
        row("ORDERED RACE ALTERNATIVES", Ui::Accent);
        row(m_live ? "From the human's current progress through remaining checkpoints." : "Preserves checkpoint order. Select a candidate, then apply its driving preset.", Ui::Muted);
        if (m_raceAlternatives.candidates.count == 0) row("No valid race-lap candidate.", Ui::Danger);
        for (int i = 0; i < m_raceAlternatives.candidates.count; ++i)
        {
            const RouteCandidateView& candidate = m_raceAlternatives.candidates[i];
            snprintf(line, sizeof(line), "%s / cost %.3f", candidate.label, candidate.totalCost);
            Ui::Button(40 + i, { x, y, width, 34 }, line, true, m_raceCandidate && m_candidate == i);
            y += 44;
            snprintf(line, sizeof(line), "%.1f distance / %.1fs / %.1f fuel", candidate.totalDistance, candidate.totalTime, candidate.totalFuel);
            row(line, Ui::Muted);
        }
        Ui::Button(50, { x, y, width, 36 }, "Use selected race route", m_raceCandidate && m_candidate >= 0 && m_candidate < m_raceAlternatives.candidates.count);
        y += 54;
        row("EXPLORATION ALTERNATIVES", Ui::Gold);
        row("Start-to-Finish candidates may skip checkpoints. Never applied as race plans.", Ui::Muted);
        for (int i = 0; i < m_alternatives.candidates.count; ++i)
        {
            const RouteCandidateView& candidate = m_alternatives.candidates[i];
            snprintf(line, sizeof(line), "%s / %.3f", candidate.label, candidate.totalCost);
            Ui::Button(30 + i, { x, y, width, 34 }, line, true, !m_raceCandidate && m_candidate == i);
            y += 44;
            snprintf(line, sizeof(line), "%.1f distance / %.1fs / %.1f fuel", candidate.totalDistance, candidate.totalTime, candidate.totalFuel);
            row(line, Ui::Muted);
        }
        row("All candidates scored under the displayed preset. Overlays use exact backend road IDs.", Ui::Muted);
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
        if (edge.id >= 0 && edge.id < m_costs.count)
        {
            if (m_costs[edge.id] < 0) row("Blocked: excluded from routing.", Ui::Danger);
            else
            {
                snprintf(line, sizeof(line), "Exact scalar cost %.6f", m_costs[edge.id]);
                row(line, Ui::Accent);
            }
        }
    }
}

void ExplorerScreen::Draw(App& app)
{
    Rectangle c = app.Content();
    Ui::Button(1, { c.x, c.y, 100, 36 }, "Paddock");
    Ui::Label(c.x + 120, c.y + 5, m_info.name ? m_info.name : "No circuit", 24);
    Ui::Button(22, { c.x + c.width - 260, c.y, 110, 36 }, "Lab", app.Available(ScreenId::Lab));
    Ui::Button(m_live ? 24 : 21, { c.x + c.width - 140, c.y, 140, 36 }, m_live ? "Resume race" : "Start race", app.Available(ScreenId::Race) && app.state.dataReady && (m_live || !app.state.raceStarted));
    const char* tabs[] = { "Overview", "BFS", "DFS", "Routes", "Resources" };
    for (int i = 0; i < 5; ++i)
    {
        Ui::Button(10 + i, { c.x + i * (c.width / 5), c.y + 50, c.width / 5 - 8, 34 }, tabs[i], true, m_tab == i);
    }
    ArrayView<int> edges = m_tab == 3 ? m_route.edgePath : ArrayView<int>{};
    ArrayView<int> vertices = m_tab == 1 ? m_bfs.path : (m_tab == 2 ? m_cycle.witnessCycle : ArrayView<int>{});
    RouteCompareView candidates = m_raceCandidate ? m_raceAlternatives : m_alternatives;
    if (m_tab == 3 && m_candidate >= 0 && m_candidate < candidates.candidates.count)
    {
        edges = candidates.candidates[m_candidate].edgePath;
        vertices = candidates.candidates[m_candidate].vertexPath;
    }
    m_map.Draw(m_graph, edges, vertices, nullptr, false, ApiGetSettings().showCostLabels ? m_costs : ArrayView<double>{});
    Ui::Card(m_details);
    Ui::BeginClip({ m_details.x, m_details.y, m_details.width, m_details.height - 26 });
    float y = m_details.y + 16 - m_scroll;
    Details(app, m_details.x + 16, y, m_details.width - 32);
    m_detailHeight = y - m_details.y + m_scroll;
    Ui::EndClip();
    if (m_detailHeight > m_details.height) Ui::Label(m_details.x + 12, m_details.y + m_details.height - 19, "Scroll / PgUp / PgDn for more", 14, Ui::Gold);
}
