#include "LabScreen.h"
#include "App.h"
#include "ui/Theme.h"
#include "ui/Format.h"
#include "ui/LabRenderers.h"
#include "ui/LabSource.h"
#include "raygui.h"
#include <cstdio>

namespace
{
    const char* kNames[] = { "BFS", "DFS", "Dijkstra", "A*", "Kruskal MST", "Track generator", "Knapsack DP", "Greedy pickup", "Refuelling DP", "Merge sort", "Quick sort", "Heap sort", "Event priority queue" };
}

void LabScreen::Enter(App& app)
{
    m_input = ApiLabDefaultInput();
    m_input.useRaceState = app.state.raceStarted && !ApiRaceOver();
    m_preset = static_cast<int>(m_input.preset);
    m_ready = false;
    m_playing = false;
    m_step = -1;
    m_scroll = 0;
    m_codeScroll = 0;
    m_displayedModule = m_module;
    m_frame = {};
    m_result[0] = '\0';
    m_runInfo = {};
    m_hasEndpointSummary = false;
}

void LabScreen::Run(App& app)
{
    m_input.preset = static_cast<Preset>(m_preset);
    m_input.seed = static_cast<unsigned long long>(m_seed);
    m_input.generatorVertexCount = m_vertices;
    m_ready = app.Check(ApiLabRun(static_cast<LabAlgo>(m_module), m_input));
    m_playing = false;
    m_step = -1;
    m_scrub = -1;
    m_tableRow = m_tableColumn = m_eventOffset = 0;
    m_codeScroll = 0;
    if (!m_ready) return;
    snprintf(m_result, sizeof(m_result), "%s", ApiLabResultText());
    m_runInfo = ApiLabRunInfo();
    snprintf(m_sourceDescription, sizeof(m_sourceDescription), "%s", m_runInfo.description ? m_runInfo.description : "");
    m_runInfo.description = m_sourceDescription;
    m_hasEndpointSummary = m_module == 2 || m_module == 3;
    if (m_hasEndpointSummary)
    {
        m_endpointSummary = ApiPlanRoute(m_runInfo.sourceVertex, m_runInfo.targetVertex, m_input.preset, m_module == 2 ? RouteAlgo::Dijkstra : RouteAlgo::AStar);
        // Keep only scalar fields; borrowed paths belong to the route query.
        m_endpointSummary.vertexPath = {};
        m_endpointSummary.edgePath = {};
    }
    m_graph = ApiLabGraph();
    m_map.Fit(m_graph);
    m_code = ApiLabPseudocode(static_cast<LabAlgo>(m_module));
    m_events = ApiLabEvents();
    m_generator = ApiLabGeneratorReport();
}

void LabScreen::Update(App& app, float dt)
{
    int action = Ui::TakeAction();
    if (action == 1)
    {
        m_playing = false;
        app.Go(app.state.labReturn);
        return;
    }
    if (action == 2) Run(app);
    if (action == 3) m_step = -1;
    if (action == 4) --m_step;
    if (action == 5)
    {
        if (m_step >= ApiLabStepCount() - 1) m_step = -1;
        m_playTime = 0;
        m_playing = !m_playing;
    }
    if (action == 6) ++m_step;
    if (action == 7) m_step = ApiLabStepCount() - 1;
    if (action == 8 && !app.SessionLocked())
    {
        if (app.Check(ApiLabSaveGenerated("generated_track.trk"))) app.toast.Show("Generated circuit saved as generated_track.trk.");
    }
    if (action == 9 && !app.SessionLocked())
    {
        if (app.Check(ApiLabSaveGenerated("generated_track.trk")))
        {
            bool loaded = app.Check(ApiLoadCircuitFile("generated_track.trk"));
            // Both successful and failed loads reset the backend Lab storage.
            // Drop every borrowed Lab view before drawing another frame.
            m_ready = false;
            m_playing = false;
            m_graph = {};
            m_frame = {};
            app.state.dataReady = ApiGetGraph().vertices.count > 0;
            app.state.raceStarted = false;
            if (loaded)
            {
                app.state.selectedCircuit = -1;
                app.state.championship = false;
                app.Go(ScreenId::Explorer);
                return;
            }
        }
    }
    if (action == 10)
    {
        m_module = (m_module + 12) % 13;
        m_scroll = 0;
    }
    if (action == 11)
    {
        m_module = (m_module + 1) % 13;
        m_scroll = 0;
    }
    if (action == 12) m_input.useBriefExample = !m_input.useBriefExample;
    if (action == 13)
    {
        m_input.useRaceState = !m_input.useRaceState;
        if (m_input.useRaceState) m_input.useBriefExample = false;
    }
    if (m_displayedModule != m_module)
    {
        m_displayedModule = m_module;
        m_ready = false;
        m_playing = false;
        m_result[0] = '\0';
    }
    if (action >= 3 && action <= 7) m_scrub = static_cast<float>(m_step);
    if (m_ready)
    {
        m_step = static_cast<int>(m_scrub);
        if (m_playing)
        {
            if (dt > 0.25f) dt = 0.25f;
            m_playTime += dt * m_rate;
            int steps = static_cast<int>(m_playTime);
            m_playTime -= steps;
            m_step += steps;
        }
        int last = ApiLabStepCount() - 1;
        if (m_step < -1) m_step = -1;
        if (m_step > last) m_step = last;
        if (m_step == last) m_playing = false;
        m_scrub = static_cast<float>(m_step);
        m_frame = ApiLabFrame(m_step);
    }
    Rectangle c = app.Content();
    float side = c.width > 1000 ? 320.0f : 260.0f;
    m_details = { c.x, c.y + 58, side, c.height - 120 };
    m_visual = { c.x + side + 16, c.y + 58, c.width - side - 16, (c.height - 136) * 0.58f };
    m_pseudocode = { m_visual.x, m_visual.y + m_visual.height + 12, m_visual.width, (c.height - 136) * 0.42f };
    m_map.bounds = m_visual;
    m_map.Update();
    if (CheckCollisionPointRec(GetMousePosition(), m_details)) m_scroll -= GetMouseWheelMove() * 40;
    if (IsKeyPressed(KEY_PAGE_DOWN)) m_scroll += 180;
    if (IsKeyPressed(KEY_PAGE_UP)) m_scroll -= 180;
    float max = m_detailHeight - m_details.height + 25;
    if (max < 0) max = 0;
    if (m_scroll < 0) m_scroll = 0;
    if (m_scroll > max) m_scroll = max;
    if (CheckCollisionPointRec(GetMousePosition(), m_pseudocode)) m_codeScroll -= GetMouseWheelMove() * 30;
    float codeMax = m_code.lines.count * 25.0f - m_pseudocode.height + 50;
    if (codeMax < 0) codeMax = 0;
    if (m_codeScroll < 0) m_codeScroll = 0;
    if (m_codeScroll > codeMax) m_codeScroll = codeMax;
}

void LabScreen::Inputs(App& app, float x, float& y, float width)
{
    char line[256];
    auto row = [&](const char* text, Color color = Ui::Muted)
    {
        float next = Ui::Wrap({ x, y, width, 240 }, text, 16, color);
        y = next + 12;
    };
    row("INPUTS / CURRENT TRACK", Ui::Accent);
    Ui::Number({ x + 90, y, width - 90, 32 }, "Steps/sec", &m_rate, 1, 120);
    y += 46;
    if (m_module <= 3)
    {
        int max = ApiGetGraph().vertices.count - 1;
        if (max < 0) max = 0;
        Ui::Number({ x + 90, y, width - 90, 32 }, "Source", &m_input.source, -1, max);
        y += 46;
        Ui::Number({ x + 90, y, width - 90, 32 }, "Target", &m_input.target, -1, max);
        y += 46;
        row("-1 uses backend defaults. BFS minimizes road count; weighted searches optimize scalar cost.");
    }
    if (m_module == 2 || m_module == 3 || (m_module >= 6 && m_module <= 8))
    {
        Ui::Number({ x + 90, y, width - 90, 32 }, "Preset", &m_preset, 0, 3);
        y += 44;
        row(Format::PresetName(static_cast<Preset>(m_preset)), Ui::Gold);
    }
    if (m_module == 6 || m_module == 7)
    {
        Ui::Number({ x + 95, y, width - 95, 32 }, "Budget .1s", &m_input.budgetTenthsSec, 0, 2000);
        y += 44;
        Ui::Button(12, { x, y, width, 34 }, m_input.useBriefExample ? "Input: brief A/B/C example" : "Input: circuit pickups");
        y += 48;
        row("Brief example: budget 10; DP 48, greedy 30. Circuit mode can fall back to the example; the source badge identifies it.");
    }
    if (m_module >= 6 && m_module <= 8)
    {
        Ui::Button(13, { x, y, width, 34 }, m_input.useRaceState ? "Resources: live race requested" : "Resources: pre-race baseline");
        y += 48;
        row("Live mode uses current fuel, nitro and remaining laps while a race runs. Otherwise the backend reports baseline or example fallback.", Ui::Muted);
    }
    if (m_module == 5 || (m_module >= 9 && m_module <= 11))
    {
        Ui::Number({ x + 90, y, width - 90, 32 }, "Seed", &m_seed, 0, 1000000);
        y += 46;
        if (m_module == 5)
        {
            Ui::Number({ x + 90, y, width - 90, 32 }, "Vertices", &m_vertices, 16, 64);
        }
        else Ui::Number({ x + 90, y, width - 90, 32 }, "Array size", &m_input.arraySize, 2, 64);
        y += 46;
    }
    if (m_module >= 9 && m_module <= 11 && ApiChampionshipGetStatus().active)
    {
        row("Sorting the most recent backend championship records, including a completed championship. Array keys are racer IDs, not rank values.", Ui::Gold);
    }
    Ui::Button(2, { x, y, width, 38 }, "Run algorithm", true, true);
    y += 55;
    if (!m_ready)
    {
        row("Run once, then replay the recorded steps. The Lab invokes the gameplay implementation.");
        return;
    }
    row(LabSource::Name(m_runInfo.source), m_runInfo.source == LabDataSource::BriefExampleFallback ? Ui::Gold : Ui::Accent);
    row(m_sourceDescription);
    row(m_result, Ui::Accent);
    if (m_hasEndpointSummary)
    {
        if (m_endpointSummary.found)
        {
            snprintf(line, sizeof(line), "V%d -> V%d: scalar %.6f", m_runInfo.sourceVertex, m_runInfo.targetVertex, m_endpointSummary.totalCost);
            row(line, Ui::Accent);
            snprintf(line, sizeof(line), "Distance %.1f / time %.1fs / fuel %.1f", m_endpointSummary.totalDistance, m_endpointSummary.totalTime, m_endpointSummary.totalFuel);
            row(line);
            snprintf(line, sizeof(line), "Search expanded %lld / relaxed %lld", m_endpointSummary.expanded, m_endpointSummary.relaxed);
            row(line);
        }
        else row("Selected endpoint is unreachable on the current graph.", Ui::Danger);
    }
    row("TRACE COUNTERS (not repeated benchmark timings)", Ui::Gold);
    snprintf(line, sizeof(line), "Visited %lld / examined %lld / relaxed %lld", m_frame.visited, m_frame.examined, m_frame.relaxed);
    row(line);
    snprintf(line, sizeof(line), "Heap pushes %lld / pops %lld", m_frame.heapPushes, m_frame.heapPops);
    row(line);
    snprintf(line, sizeof(line), "Comparisons %lld / moves %lld", m_frame.comparisons, m_frame.swaps);
    row(line);
    snprintf(line, sizeof(line), "DP cells %lld / placements %lld", m_frame.tableCellsFilled, m_frame.placements);
    row(line);
    snprintf(line, sizeof(line), "Backtracks %lld / rejections %lld", m_frame.backtracks, m_frame.rejections);
    row(line);
    if (m_frame.kind == LabFrameKind::Table)
    {
        Ui::Number({ x + 90, y, width - 90, 32 }, "Table row", &m_tableRow, 0, m_frame.tableRows > 0 ? m_frame.tableRows - 1 : 0);
        y += 44;
        Ui::Number({ x + 90, y, width - 90, 32 }, "Column", &m_tableColumn, 0, m_frame.tableCols > 0 ? m_frame.tableCols - 1 : 0);
        y += 44;
    }
    if (m_frame.kind == LabFrameKind::Events)
    {
        Ui::Number({ x + 90, y, width - 90, 32 }, "Event row", &m_eventOffset, 0, m_events.count > 0 ? m_events.count - 1 : 0);
        y += 44;
    }
    if (m_frame.selectedIds.count > 0) row("Selected / applied IDs:", Ui::Accent);
    for (int i = 0; i < m_frame.selectedIds.count; ++i)
    {
        snprintf(line, sizeof(line), "%d", m_frame.selectedIds[i]);
        row(line);
    }
    if (m_frame.rejectedIds.count > 0) row("Rejected / cancelled IDs:", Ui::Danger);
    for (int i = 0; i < m_frame.rejectedIds.count; ++i)
    {
        snprintf(line, sizeof(line), "%d", m_frame.rejectedIds[i]);
        row(line);
    }
    if (m_module == 5)
    {
        row("GENERATION ATTEMPTS", Ui::Gold);
        for (int i = 0; i < m_generator.attempts.count; ++i)
        {
            const GeneratorAttemptView& a = m_generator.attempts[i];
            snprintf(line, sizeof(line), "Attempt %d: %s / %lld explored / %lld pruned", a.attempt, a.accepted ? "accepted" : "rejected", a.nodesExplored, a.prunedBranches);
            row(line);
            if (!a.accepted) row(a.reason, Ui::Danger);
        }
        bool canSave = m_generator.acceptedAttempt >= 0 && !app.SessionLocked();
        Ui::Button(8, { x, y, width, 34 }, "Save generated circuit", canSave);
        y += 46;
        Ui::Button(9, { x, y, width, 34 }, "Save and explore", canSave);
        y += 46;
        if (app.SessionLocked()) row("Loading a generated circuit is locked during the current session.");
    }
    if (m_frame.kind == LabFrameKind::Graph)
    {
        row(m_module == 5 ? "Generator node roles: 1 Start, 2 Finish, 3 CP, 4 fuel, 5 nitro." : "Node states: 1 frontier, 2 settled/visited, 3 DFS finished.", Ui::Gold);
        for (int i = 0; i < m_frame.vertexValue.count; ++i)
        {
            if (m_frame.vertexValue[i] < 0) continue;
            snprintf(line, sizeof(line), "V%d / value %.3f / parent %d", i, m_frame.vertexValue[i], i < m_frame.vertexParent.count ? m_frame.vertexParent[i] : -1);
            row(line);
        }
    }
}

void LabScreen::Draw(App& app)
{
    Rectangle c = app.Content();
    Ui::Button(1, { c.x, c.y, 90, 34 }, "Back");
    Ui::Button(10, { c.x + 110, c.y, 46, 34 }, "<");
    Ui::Label(c.x + 172, c.y + 7, kNames[m_module], 23, Ui::Accent);
    Ui::Button(11, { c.x + c.width - 46, c.y, 46, 34 }, ">");
    Ui::Card(m_details);
    Ui::BeginClip(m_details);
    float y = m_details.y + 14 - m_scroll;
    Inputs(app, m_details.x + 14, y, m_details.width - 28);
    m_detailHeight = y - m_details.y + m_scroll;
    Ui::EndClip();
    if (m_ready)
    {
        if (m_frame.kind == LabFrameKind::Graph)
        {
            int endpoints[] = { m_runInfo.sourceVertex, m_runInfo.targetVertex };
            m_map.Draw(m_graph, {}, { endpoints, 2 }, &m_frame, m_module == 5);
            Ui::Scissor(static_cast<int>(m_visual.x), static_cast<int>(m_visual.y), static_cast<int>(m_visual.width), static_cast<int>(m_visual.height));
            for (int i = 0; i < m_graph.vertices.count; ++i)
            {
                const VertexView& v = m_graph.vertices[i];
                bool source = v.id == m_runInfo.sourceVertex;
                bool target = v.id == m_runInfo.targetVertex;
                if (!source && !target) continue;
                Vector2 p = m_map.Point(v.x, v.y);
                DrawCircleLinesV(p, 14, source ? Ui::Accent : Ui::Gold);
                Ui::Label(p.x + 12, p.y + 12, source && target ? "SOURCE = TARGET" : (source ? "SOURCE" : "TARGET"), 14, Ui::Text);
            }
            EndScissorMode();
        }
        if (m_frame.kind == LabFrameKind::Array) LabRenderers::Array(m_visual, m_frame, m_runInfo.source == LabDataSource::ChampionshipStandings);
        if (m_frame.kind == LabFrameKind::Table) LabRenderers::Table(m_visual, m_frame, m_tableRow, m_tableColumn);
        if (m_frame.kind == LabFrameKind::Events) LabRenderers::Events(m_visual, m_frame, m_events, m_eventOffset);
        Ui::Card(m_pseudocode);
        Ui::Scissor(static_cast<int>(m_pseudocode.x), static_cast<int>(m_pseudocode.y), static_cast<int>(m_pseudocode.width), static_cast<int>(m_pseudocode.height));
        for (int i = 0; i < m_code.lines.count; ++i)
        {
            float ly = m_pseudocode.y + 15 + i * 25 - m_codeScroll;
            if (i == m_frame.line) DrawRectangleRec({ m_pseudocode.x + 4, ly - 2, m_pseudocode.width - 8, 24 }, Ui::Border);
            Ui::Label(m_pseudocode.x + 12, ly, m_code.lines[i], 16, i == m_frame.line ? Ui::Accent : Ui::Muted);
        }
        EndScissorMode();
    }
    else
    {
        Ui::Card(m_visual);
        Ui::Wrap({ m_visual.x + 20, m_visual.y + 30, m_visual.width - 40, m_visual.height - 40 }, "Choose a module with the arrows. Configure inputs and run to inspect its real trace.", 22, Ui::Muted);
    }
    float bottom = c.y + c.height - 46;
    const char* labels[] = { "|<", "<", m_playing ? "Pause" : "Play", ">", ">|" };
    for (int i = 0; i < 5; ++i) Ui::Button(3 + i, { c.x + i * 56, bottom, 48, 34 }, labels[i], m_ready);
    float minX = c.x + 300;
    if (m_ready)
    {
        GuiSliderBar({ minX, bottom + 7, c.width - 420, 20 }, nullptr, nullptr, &m_scrub, -1, static_cast<float>(ApiLabStepCount() - 1));
        char step[64];
        snprintf(step, sizeof(step), "%d / %d", m_step, ApiLabStepCount() - 1);
        Ui::Label(c.x + c.width - 108, bottom + 8, step, 16, Ui::Accent);
    }
}
