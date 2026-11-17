#include "ChampionshipScreen.h"
#include "App.h"
#include "ui/Theme.h"
#include "ui/Format.h"
#include <cstdio>

void ChampionshipScreen::Enter(App&)
{
    m_dirty = true;
    m_lastK = -1;
    m_lastHistory = -1;
    m_scroll = 0;
}

void ChampionshipScreen::Update(App& app, float)
{
    int action = Ui::TakeAction();
    if (action == 1)
    {
        app.Go(ScreenId::Menu);
        return;
    }
    if (action == 2)
    {
        if (!app.state.dataReady && !app.LoadCircuit(m_status.currentCircuitIndex)) return;
        app.state.raceRecorded = false;
        app.Go(ScreenId::Explorer);
        return;
    }
    if (action >= 10 && action <= 12)
    {
        m_method = action - 10;
        m_dirty = true;
    }
    if (action == 3) m_dirty = true;
    if (action == 20 || action == 21)
    {
        m_page = action - 20;
        m_scroll = 0;
    }
    Rectangle c = app.Content();
    m_area = { c.x, c.y + 162, c.width, c.height - 172 };
    if (CheckCollisionPointRec(GetMousePosition(), m_area)) m_scroll -= GetMouseWheelMove() * 40;
    if (IsKeyPressed(KEY_PAGE_DOWN)) m_scroll += 180;
    if (IsKeyPressed(KEY_PAGE_UP)) m_scroll -= 180;
    float max = m_height - m_area.height + 20;
    if (max < 0) max = 0;
    if (m_scroll < 0) m_scroll = 0;
    if (m_scroll > max) m_scroll = max;
    m_status = ApiChampionshipGetStatus();
    if (m_dirty || m_lastK != m_k || m_lastHistory != m_historyIndex)
    {
        m_standings = ApiStandings(static_cast<SortMethod>(m_method));
        m_top = ApiTopK(m_k);
        m_history = m_status.racesCompleted > 0 ? ApiRaceHistory(m_historyIndex) : ResultsView{};
        m_reason = ApiTieBreakReason(m_racerA, m_racerB);
        m_lastK = m_k;
        m_lastHistory = m_historyIndex;
        m_dirty = false;
    }
}

void ChampionshipScreen::Draw(App& app)
{
    Rectangle c = app.Content();
    Ui::Button(1, { c.x, c.y, 130, 36 }, "Paddock");
    Ui::Button(2, { c.x + c.width - 220, c.y, 220, 36 }, "Plan next race", !m_status.complete);
    char line[256];
    if (m_status.complete) snprintf(line, sizeof(line), "CHAMPION / %s", m_status.championId >= 0 && m_status.championId < 10 ? app.state.racerNames[m_status.championId] : "?");
    else snprintf(line, sizeof(line), "%d / %d RACES COMPLETE", m_status.racesCompleted, m_status.racesTotal);
    Ui::Label(c.x, c.y + 60, line, 28, Ui::Accent);
    Ui::Button(20, { c.x, c.y + 110, 230, 34 }, "Standings and top-k", true, m_page == 0);
    Ui::Button(21, { c.x + 245, c.y + 110, 230, 34 }, "History and tie-breaks", true, m_page == 1);
    Ui::Card(m_area);
    Ui::BeginClip(m_area);
    float x = m_area.x + 16;
    float width = m_area.width - 32;
    float y = m_area.y + 16 - m_scroll;
    if (m_page == 0)
    {
        const char* sorts[] = { "Merge sort", "Quick sort", "Heap sort" };
        for (int i = 0; i < 3; ++i) Ui::Button(10 + i, { x + i * 145, y, 135, 34 }, sorts[i], true, m_method == i);
        y += 56;
        snprintf(line, sizeof(line), "%lld comparisons / %lld moves / %.2f us", m_standings.comparisons, m_standings.swaps, m_standings.micros);
        Ui::Label(x, y, line, 17, Ui::Muted);
        y += 32;
        Ui::Label(x, y, m_standings.allMethodsAgree ? "All three rankings agree" : "Ranking disagreement", 18, m_standings.allMethodsAgree ? Ui::Accent : Ui::Danger);
        y += 40;
        float posX = x, driverX = x + width * 0.08f, pointsX = x + width * 0.36f;
        float winsX = x + width * 0.50f, sumX = x + width * 0.63f, lapX = x + width * 0.78f;
        Ui::Label(posX, y, "POS", 15, Ui::Muted);
        Ui::Label(driverX, y, "DRIVER", 15, Ui::Muted);
        Ui::Label(pointsX, y, "POINTS", 15, Ui::Muted);
        Ui::Label(winsX, y, "WINS", 15, Ui::Muted);
        Ui::Label(sumX, y, "SUM POS", 15, Ui::Muted);
        Ui::Label(lapX, y, "BEST LAP", 15, Ui::Muted);
        y += 34;
        for (int i = 0; i < m_standings.rows.count; ++i)
        {
            const StandingRowView& r = m_standings.rows[i];
            Color color = i < m_top.rows.count ? Ui::Accent : Ui::Text;
            snprintf(line, sizeof(line), "%d", r.position);
            Ui::Label(posX, y, line, 18, color);
            Ui::Label(driverX, y, r.name, 18, color);
            snprintf(line, sizeof(line), "%d", r.totalPoints);
            Ui::Label(pointsX, y, line, 18, color);
            snprintf(line, sizeof(line), "%d", r.wins);
            Ui::Label(winsX, y, line, 18, color);
            snprintf(line, sizeof(line), "%d", r.sumOfFinishPositions);
            Ui::Label(sumX, y, line, 18, color);
            Format::Time(line, sizeof(line), r.bestFastestLapMs);
            Ui::Label(lapX, y, line, 17, color);
            y += 34;
        }
        y += 24;
        Ui::Number({ x + 120, y, 160, 34 }, "Top k", &m_k, 1, m_standings.rows.count > 0 ? m_standings.rows.count : 1);
        y += 52;
        Ui::Label(x, y, m_top.matchesSortPrefix ? "Top-k matches the full-ranking prefix" : "Top-k mismatch", 19, m_top.matchesSortPrefix ? Ui::Gold : Ui::Danger);
        y += 34;
        for (int i = 0; i < m_top.rows.count; ++i)
        {
            snprintf(line, sizeof(line), "%d / %s / %d points", m_top.rows[i].position, m_top.rows[i].name, m_top.rows[i].totalPoints);
            Ui::Label(x, y, line, 18);
            y += 28;
        }
        snprintf(line, sizeof(line), "Heap %lld vs full-sort %lld comparisons", m_top.comparisons, m_top.fullSortComparisons);
        Ui::Label(x, y + 12, line, 17, Ui::Muted);
        y += 60;
    }
    else
    {
        Ui::Number({ x + 150, y, 160, 34 }, "History index", &m_historyIndex, 0, m_status.racesCompleted > 0 ? m_status.racesCompleted - 1 : 0);
        y += 58;
        if (m_history.rows.count == 0) Ui::Label(x, y, "No recorded races.", 18, Ui::Muted);
        for (int i = 0; i < m_history.rows.count; ++i)
        {
            const ResultRowView& r = m_history.rows[i];
            char finish[32];
            Format::Time(finish, sizeof(finish), r.finishTimeMs);
            snprintf(line, sizeof(line), "%d / Racer %d / %s / %d points", r.finishPosition, r.racerId, finish, r.pointsAwarded);
            Ui::Label(x, y, line, 18);
            y += 32;
        }
        y += 40;
        Ui::Label(x, y, "TIE-BREAK EXPLAINER", 21, Ui::Accent);
        y += 42;
        Ui::Number({ x + 150, y, 160, 34 }, "Racer A", &m_racerA, 0, m_standings.rows.count > 0 ? m_standings.rows.count - 1 : 0);
        y += 46;
        Ui::Number({ x + 150, y, 160, 34 }, "Racer B", &m_racerB, 0, m_standings.rows.count > 0 ? m_standings.rows.count - 1 : 0);
        y += 46;
        Ui::Button(3, { x, y, 310, 34 }, "Explain distinguishing criterion");
        y += 54;
        const char* criteria[] = { "Higher points", "More wins", "Lower sum of positions", "Lower best lap time", "Lower racer ID", "Same racer" };
        Ui::Label(x, y, criteria[static_cast<int>(m_reason)], 19, Ui::Gold);
        y += 42;
        Ui::Wrap({ x, y, width, 90 }, "This names the criterion. The ranked table determines which driver is ahead. A smaller racer ID is the final tie-break.", 17);
        y += 110;
    }
    m_height = y - m_area.y + m_scroll;
    Ui::EndClip();
    Ui::Label(c.x + c.width - 220, c.y + c.height - 10, "Scroll / PgUp / PgDn", 15, Ui::Gold);
}
