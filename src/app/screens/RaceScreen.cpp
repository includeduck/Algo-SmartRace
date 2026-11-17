#include "RaceScreen.h"
#include "App.h"
#include "ui/Theme.h"
#include "ui/Format.h"
#include <cstdio>

void RaceScreen::Enter(App& app)
{
    if (m_serial != app.state.raceSerial)
    {
        m_serial = app.state.raceSerial;
        m_paused = false;
        m_autopilot = true;
        m_refuelPopup = false;
        m_choiceQueued = false;
        m_waiting = false;
        m_promptVertex = -1;
        m_speed = 1;
        m_lastLog = -1;
        m_logCount = m_logHead = m_logOffset = 0;
        m_scroll = 0;
        m_graph = ApiGetGraph();
        m_map.Fit(m_graph);
    }
    m_clock.Reset();
    Pull();
}

void RaceScreen::Pull()
{
    m_snapshot = ApiRaceSnapshot();
    m_graph = ApiGetGraph();
    m_order = ApiRaceResults();
    for (int i = 0; i < m_snapshot.racers.count; ++i)
    {
        if (m_snapshot.racers[i].isHuman) m_human = m_snapshot.racers[i];
    }
}

bool RaceScreen::NeedsDecision() const
{
    return !m_autopilot && !m_waiting && !m_choiceQueued && (m_human.state == RacerState::AtVertex || m_human.state == RacerState::Waiting);
}

void RaceScreen::DecisionPoint()
{
    if (m_human.state == RacerState::OnEdge) m_promptVertex = -1;
    if (m_autopilot || m_human.state != RacerState::AtVertex || m_promptVertex == m_human.currentVertex) return;
    for (int i = 0; i < m_graph.vertices.count; ++i)
    {
        const VertexView& vertex = m_graph.vertices[i];
        if (vertex.id != m_human.currentVertex || vertex.type != VertexType::FuelStation || !vertex.active) continue;
        m_purchase = ApiRecommendedRefuel();
        int capacity = 1000 - m_human.fuelTenths;
        if (m_purchase > capacity) m_purchase = capacity;
        if (m_purchase > 400) m_purchase = 400;
        m_refuelPopup = true;
        m_promptVertex = vertex.id;
    }
}

void RaceScreen::Logs()
{
    // ApiRaceLogSince returns at most 64 lines; copy before another call.
    for (int batch = 0; batch < 8; ++batch)
    {
        LogView log = ApiRaceLogSince(m_lastLog);
        int count = log.lines.count;
        if (count == 0) break;
        for (int i = 0; i < count; ++i)
        {
            char time[32];
            Format::Time(time, sizeof(time), log.lines[i].timeMs);
            snprintf(m_log[m_logHead], 256, "%s  %s", time, log.lines[i].text);
            m_logHead = (m_logHead + 1) % 200;
            if (m_logCount < 200) ++m_logCount;
            m_lastLog = log.lines[i].index;
        }
        if (count < 64) break;
    }
}

void RaceScreen::Update(App& app, float dt)
{
    int action = Ui::TakeAction();
    if (IsKeyPressed(KEY_P)) action = 2;
    if (IsKeyPressed(KEY_A)) action = 3;
    if (IsKeyPressed(KEY_N)) action = 4;
    if (IsKeyPressed(KEY_L)) action = 5;
    if (action == 1)
    {
        m_clock.Reset();
        app.Go(ScreenId::Menu);
        return;
    }
    if (action == 5)
    {
        m_clock.Reset();
        app.state.labReturn = ScreenId::Race;
        app.Go(ScreenId::Lab);
        return;
    }
    if (action == 2)
    {
        m_paused = !m_paused;
        m_clock.Reset();
    }
    if (action == 3 && app.Check(ApiCmdAutopilot(!m_autopilot)))
    {
        m_autopilot = !m_autopilot;
        m_choiceQueued = false;
        m_waiting = false;
        m_refuelPopup = false;
        m_clock.Reset();
    }
    if (action == 4) app.Check(ApiCmdNitro());
    if (action == 6) m_speed = m_speed == 8 ? 1 : m_speed * 2;
    if (action == 7 && app.Check(ApiCmdRefuel(m_purchase))) m_refuelPopup = false;
    if (action == 8) m_refuelPopup = false;
    if (action == 9 && m_logOffset < m_logCount - 1) ++m_logOffset;
    if (action == 10 && m_logOffset > 0) --m_logOffset;
    if (action == 12)
    {
        m_waiting = !m_waiting;
        m_clock.Reset();
    }
    if (action >= 1000 && app.Check(ApiCmdChooseEdge(action - 1000)))
    {
        m_choiceQueued = true;
        m_waiting = false;
        m_clock.Reset();
    }
    Pull();
    DecisionPoint();
    int ticks = 0;
    if (!m_refuelPopup && !NeedsDecision())
    {
        if (action == 11) ticks = 1;
        else if (!m_paused) ticks = m_clock.Accumulate(dt, m_speed);
    }
    for (int i = 0; i < ticks; ++i)
    {
        ApiRaceStep(1);
        m_choiceQueued = false;
        Pull();
        DecisionPoint();
        if (ApiRaceOver() || m_refuelPopup || NeedsDecision())
        {
            m_clock.Reset();
            break;
        }
    }
    Logs();
    if (ApiRaceOver())
    {
        app.CaptureResults();
        app.Go(ScreenId::Results);
        return;
    }
    Rectangle c = app.Content();
    float side = c.width < 1000 ? 270.0f : 335.0f;
    m_map.bounds = { c.x, c.y + 104, c.width - side - 20, c.height - 114 };
    m_details = { m_map.bounds.x + m_map.bounds.width + 20, m_map.bounds.y, side, m_map.bounds.height };
    if (CheckCollisionPointRec(GetMousePosition(), m_details)) m_scroll -= GetMouseWheelMove() * 40;
    if (IsKeyPressed(KEY_PAGE_DOWN)) m_scroll += 180;
    if (IsKeyPressed(KEY_PAGE_UP)) m_scroll -= 180;
    float max = m_detailHeight - m_details.height + 20;
    if (max < 0) max = 0;
    if (m_scroll < 0) m_scroll = 0;
    if (m_scroll > max) m_scroll = max;
    m_map.Update();
}

void RaceScreen::Draw(App& app)
{
    Rectangle c = app.Content();
    const char* controls[] = { "Paddock", m_paused ? "Resume [P]" : "Pause [P]", m_autopilot ? "Manual [A]" : "Autopilot [A]", "Nitro [N]", "Lab [L]" };
    float bw = (c.width - 40) / 5;
    for (int i = 0; i < 5; ++i)
    {
        Ui::Button(i + 1, { c.x + i * (bw + 10), c.y, bw, 36 }, controls[i], i != 3 || (m_human.nitroCharges > 0 && !m_human.nitroActive));
    }
    char line[256], time[32];
    Format::Time(time, sizeof(time), m_snapshot.timeMs);
    snprintf(line, sizeof(line), "%s  /  Lap %d of %d  /  Next CP %d", time, m_human.lap, ApiGetSettings().laps, m_human.nextCheckpoint);
    Ui::Label(c.x, c.y + 62, line, 20, Ui::Accent);
    m_map.Draw(m_graph);
    m_map.Cars(m_snapshot);
    float x = m_map.bounds.x + m_map.bounds.width + 20;
    float width = c.x + c.width - x;
    Ui::BeginClip(m_details);
    float y = c.y + 108 - m_scroll;
    snprintf(line, sizeof(line), "FUEL  %.1f / 100", m_human.fuelTenths / 10.0);
    Ui::Label(x, y, line, 19);
    Ui::Bar({ x, y + 28, width, 10 }, m_human.fuelTenths / 1000.0f, m_human.fuelTenths < 250 ? Ui::Danger : Ui::Accent);
    snprintf(line, sizeof(line), "NITRO  %d / 3%s", m_human.nitroCharges, m_human.nitroActive ? "  ACTIVE" : "");
    Ui::Label(x, y + 56, line, 19, Ui::Gold);
    Ui::Label(x, y + 92, Format::State(m_human.state), 18, Ui::Muted);
    snprintf(line, sizeof(line), "%dx playback", m_speed);
    Ui::Button(6, { x, y + 128, width / 2 - 5, 34 }, line);
    Ui::Button(11, { x + width / 2 + 5, y + 128, width / 2 - 5, 34 }, "1 tick", m_paused && !NeedsDecision());
    if (!m_autopilot) Ui::Button(12, { x, y + 171, width, 32 }, m_waiting ? "Stop waiting / choose road" : "Wait for a road to reopen");
    y += m_autopilot ? 182 : 220;
    if (m_refuelPopup)
    {
        Ui::Label(x, y, "FUEL STATION", 19, Ui::Gold);
        Ui::Number({ x + 90, y + 35, width - 90, 34 }, "Tenths", &m_purchase, 0, (1000 - m_human.fuelTenths) < 400 ? 1000 - m_human.fuelTenths : 400);
        Ui::Button(7, { x, y + 82, width / 2 - 5, 36 }, "Purchase");
        Ui::Button(8, { x + width / 2 + 5, y + 82, width / 2 - 5, 36 }, "Skip");
        y += 140;
    }
    else if (NeedsDecision())
    {
        Ui::Label(x, y, "CHOOSE YOUR NEXT ROAD", 18, Ui::Gold);
        y += 34;
        for (int i = 0; i < m_graph.edges.count; ++i)
        {
            const EdgeView& e = m_graph.edges[i];
            bool from = e.from == m_human.currentVertex || (!e.directed && e.to == m_human.currentVertex);
            if (!from) continue;
            int target = e.from == m_human.currentVertex ? e.to : e.from;
            snprintf(line, sizeof(line), "E%d -> V%d%s", e.id, target, e.status == EdgeStatus::Blocked ? "  BLOCKED" : "");
            Ui::Button(1000 + e.id, { x, y, width, 32 }, line, e.status != EdgeStatus::Blocked);
            y += 40;
        }
    }
    else
    {
        Ui::Label(x, y, "RUNNING ORDER", 18, Ui::Accent);
        y += 30;
        for (int i = 0; i < m_order.rows.count; ++i)
        {
            const ResultRowView& r = m_order.rows[i];
            snprintf(line, sizeof(line), "%d   %s", r.finishPosition, r.racerId >= 0 && r.racerId < 10 ? app.state.racerNames[r.racerId] : "?");
            Ui::Label(x, y, line, 17);
            y += 25;
        }
    }
    y += 22;
    if (m_logCount > 0)
    {
        Ui::Button(9, { x, y, width / 2 - 5, 30 }, "Older log", m_logCount > m_logOffset + 1);
        Ui::Button(10, { x + width / 2 + 5, y, width / 2 - 5, 30 }, "Newer", m_logOffset > 0);
        y += 42;
        int available = 8;
        for (int i = m_logOffset; i < m_logCount && i < m_logOffset + available; ++i)
        {
            int index = (m_logHead - 1 - i + 200) % 200;
            Ui::Wrap({ x, y, width, 48 }, m_log[index], 15);
            y += 52;
        }
    }
    m_detailHeight = y - m_details.y + m_scroll;
    Ui::EndClip();
    if (m_paused || NeedsDecision() || m_refuelPopup)
    {
        Ui::Label(m_map.bounds.x + 18, m_map.bounds.y + 42, m_paused ? "PAUSED" : "AWAITING YOUR DECISION", 22, Ui::Gold);
    }
}

void ResultsScreen::Update(App& app, float)
{
    int action = Ui::TakeAction();
    m_scroll -= GetMouseWheelMove() * 40;
    float max = app.state.resultCount * 36.0f - (app.Content().height - 155);
    if (max < 0) max = 0;
    if (m_scroll < 0) m_scroll = 0;
    if (m_scroll > max) m_scroll = max;
    if (action == 1) app.Go(ScreenId::Menu);
    if (action == 2 && !app.state.championship)
    {
        if (app.state.selectedCircuit >= 0 && app.LoadCircuit(app.state.selectedCircuit)) app.StartRace();
    }
    if (action == 3 && !app.state.raceRecorded && app.state.championship)
    {
        int before = ApiChampionshipGetStatus().racesCompleted;
        ApiStatus result = ApiChampionshipNextRace();
        char error[512] = {};
        if (result != ApiStatus::Ok) snprintf(error, sizeof(error), "%s", ApiLastError());
        ChampionshipStatus after = ApiChampionshipGetStatus();
        // Recording can succeed before loading the next circuit fails.
        // Observe progress to prevent an error retry from awarding twice.
        if (after.racesCompleted > before)
        {
            app.state.raceRecorded = true;
            app.state.raceStarted = false;
            app.state.selectedCircuit = after.currentCircuitIndex;
            app.state.dataReady = ApiGetGraph().vertices.count > 0;
            if (result != ApiStatus::Ok) app.toast.Show(error);
            app.Go(ScreenId::Championship);
        }
        else if (result != ApiStatus::Ok) app.toast.Show(error);
    }
}

void ResultsScreen::Draw(App& app)
{
    Rectangle c = app.Content();
    Ui::Label(c.x, c.y, "Race classification", 32, Ui::Accent);
    Ui::Label(c.x, c.y + 58, "POS / DRIVER              STATUS          FINISH             BEST LAP       POINTS", 17, Ui::Muted);
    Ui::BeginClip({ c.x, c.y + 90, c.width, c.height - 155 });
    for (int i = 0; i < app.state.resultCount; ++i)
    {
        const ResultRowView& r = app.state.resultRows[i];
        char line[256], finish[32], lap[32];
        Format::Time(finish, sizeof(finish), r.finishTimeMs);
        Format::Time(lap, sizeof(lap), r.bestLapTimeMs);
        snprintf(line, sizeof(line), "%2d  %-12s  %-10s  %s    %s     %d", r.finishPosition, r.racerId >= 0 && r.racerId < 10 ? app.state.racerNames[r.racerId] : "?", r.status == 0 ? "Finished" : (r.status == 1 ? "DNF" : "Time limit"), finish, lap, r.pointsAwarded);
        Ui::Label(c.x, c.y + 100 + i * 36 - m_scroll, line, c.width < 950 ? 16 : 20);
    }
    Ui::EndClip();
    float y = c.y + c.height - 52;
    Ui::Button(1, { c.x, y, 170, 40 }, "Paddock");
    if (app.state.championship) Ui::Button(3, { c.x + 190, y, 250, 40 }, "Record race and continue", !app.state.raceRecorded);
    else Ui::Button(2, { c.x + 190, y, 220, 40 }, "Race again", app.state.selectedCircuit >= 0);
}
