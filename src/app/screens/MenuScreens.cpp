#include "MenuScreens.h"
#include "App.h"
#include "ui/Theme.h"
#include "ui/Format.h"
#include "raygui.h"
#include <cstdio>
#include <cstdlib>
#include <cerrno>

void MainMenuScreen::Update(App& app, float)
{
    int action = Ui::TakeAction();
    if (action == 1)
    {
        if (app.SessionLocked())
        {
            app.Go(app.state.raceStarted ? ScreenId::Race : ScreenId::Explorer);
        }
        else if (app.Check(ApiChampionshipStart()))
        {
            app.state.championship = true;
            app.state.selectedCircuit = 0;
            app.state.dataReady = true;
            app.state.raceStarted = false;
            app.Go(ScreenId::Explorer);
        }
        else app.state.dataReady = ApiGetGraph().vertices.count > 0;
    }
    if (action == 2)
    {
        app.state.championship = false;
        app.Go(ScreenId::Circuits);
    }
    if (action == 3)
    {
        app.state.labReturn = ScreenId::Menu;
        app.Go(ScreenId::Lab);
    }
    if (action == 4) app.Go(ScreenId::Instructions);
    if (action == 5) app.Go(ScreenId::Settings);
    if (action == 6) app.state.quit = true;
    if (action == 7) app.Go(ScreenId::Explorer);
    if (action == 8) app.LoadCircuit(0);
    if (action == 9) app.Go(ScreenId::Championship);
}

void MainMenuScreen::Draw(App& app)
{
    Rectangle c = app.Content();
    bool wide = c.width > 1050;
    float menuWidth = wide ? 430.0f : c.width;
    Ui::Label(c.x, c.y + 12, "EVERY TURN IS A DECISION.", wide ? 38 : 30);
    Ui::Wrap({ c.x, c.y + 68, menuWidth, 90 }, "Read the network. Plan your resources. Race a changing track.", 20);
    if (!app.state.dataReady) Ui::Button(8, { c.x + menuWidth - 140, c.y + 88, 140, 32 }, "Retry data load");
    const char* names[] = { app.SessionLocked() ? "Continue session" : "Start championship", "Quick race", "Algorithm lab", "Driver handbook", "Race configuration", "Exit" };
    float gap = c.height < 560 ? 45.0f : 53.0f;
    float start = c.height < 560 ? 125.0f : 150.0f;
    for (int i = 0; i < 6; ++i)
    {
        bool enabled = i != 1 || !app.SessionLocked();
        if (i == 0 || i == 1) enabled = enabled && app.Available(ScreenId::Race) && app.state.dataReady;
        if (i == 2) enabled = app.Available(ScreenId::Lab) && app.state.dataReady;
        if (i == 4) enabled = !app.SessionLocked();
        Ui::Button(i + 1, { c.x, c.y + start + i * gap, menuWidth, 38 }, names[i], enabled, i == 0);
    }
    if (!app.Available(ScreenId::Race)) Ui::Button(7, { c.x, c.y + 475, menuWidth, 40 }, "Explore loaded circuit", app.state.dataReady);
    if (ApiChampionshipGetStatus().active) Ui::Button(9, { c.x, c.y + start + 6 * gap, menuWidth, 32 }, "View championship standings");
    if (wide)
    {
        Rectangle r = { c.x + 475, c.y + 10, c.width - 475, c.height - 20 };
        Ui::Card(r);
        Ui::Label(r.x + 30, r.y + 32, "THE CHAMPIONSHIP", 26, Ui::Accent);
        Ui::Wrap({ r.x + 30, r.y + 92, r.width - 60, 150 }, "01  PLAN\nExplore junctions, compare weighted routes and review fuel stops.\n\n02  RACE\nReact to closures, use nitro and choose your next road.", 20);
        Ui::Wrap({ r.x + 30, r.y + 280, r.width - 60, 150 }, "03  UNDERSTAND\nReplay the same algorithms used in gameplay.\n\n04  REVIEW\nCompare rankings and retrieve the top racers with a heap.", 20);
        Ui::Label(r.x + 30, r.y + r.height - 55, "3 CIRCUITS   /   12 ALGORITHM FAMILIES", 18, Ui::Gold);
    }
}

void InstructionsScreen::Update(App& app, float)
{
    if (Ui::TakeAction() == 1) app.Go(ScreenId::Menu);
    m_scroll -= GetMouseWheelMove() * 40;
    if (IsKeyPressed(KEY_PAGE_DOWN)) m_scroll += 180;
    if (IsKeyPressed(KEY_PAGE_UP)) m_scroll -= 180;
    if (m_scroll < 0) m_scroll = 0;
    if (m_scroll > 500) m_scroll = 500;
}

void InstructionsScreen::Draw(App& app)
{
    Rectangle c = app.Content();
    Ui::Button(1, { c.x, c.y, 140, 38 }, "Back");
    Ui::Label(c.x, c.y + 64, "Plan. Race. Review.", 30, Ui::Accent);
    Ui::BeginClip({ c.x, c.y + 110, c.width, c.height - 110 });
    Ui::Wrap({ c.x, c.y + 116 - m_scroll, c.width, 1100 },
        "Visit every mandatory checkpoint in order on each lap. Cars follow graph roads; blocked roads are excluded. Restrictions change the effective cost.\n\n"
        "Fuel: start with 100 units. An active station sells up to 40 units per stop; a positive purchase takes 3 seconds. Nitro: configured start (0-3), maximum 3. Each boost gives 1.5x speed for 4 seconds with 2x fuel consumption.\n\n"
        "Race: pause or open the Lab to freeze simulation time. Use manual mode to select outgoing roads, or keep autopilot. No valid route: wait up to 60 seconds, then DNF.\n\n"
        "Ranking: 25 / 18 / 15 / 12 / 10 / 8 / 6 / 4 / 2 / 1 points; fastest-lap bonus 1. Ties: wins, sum of positions, best lap, racer ID.\n\n"
        "BFS minimizes road count, not weighted distance. Dijkstra and A* optimize the displayed scalar cost. The Lab replays backend traces; counters are not benchmark studies.", 19);
    Ui::EndClip();
}

void SettingsScreen::Enter(App&)
{
    m_draft = ApiGetSettings();
    m_preset = static_cast<int>(m_draft.defaultPreset);
    snprintf(m_seed, sizeof(m_seed), "%llu", m_draft.eventSeed);
    m_seedEdit = false;
    m_settingsScroll = 0;
}

void SettingsScreen::Update(App& app, float)
{
    int action = Ui::TakeAction();
    if (action == 3) m_draft.showCostLabels = !m_draft.showCostLabels;
    m_settingsScroll -= GetMouseWheelMove() * 40;
    if (IsKeyPressed(KEY_PAGE_DOWN)) m_settingsScroll += 150;
    if (IsKeyPressed(KEY_PAGE_UP)) m_settingsScroll -= 150;
    float max = 460 - (app.Content().height - 130);
    if (max < 0) max = 0;
    if (m_settingsScroll < 0) m_settingsScroll = 0;
    if (m_settingsScroll > max) m_settingsScroll = max;
    if (action == 1) app.Go(ScreenId::Menu);
    if (action == 2 && !app.SessionLocked())
    {
        char* end = nullptr;
        errno = 0;
        unsigned long long seed = strtoull(m_seed, &end, 10);
        bool digits = m_seed[0] != '\0';
        for (int i = 0; m_seed[i]; ++i)
        {
            if (m_seed[i] < '0' || m_seed[i] > '9') digits = false;
        }
        if (!digits || errno == ERANGE || end == nullptr || *end)
        {
            app.toast.Show("Enter a seed using unsigned decimal digits within the 64-bit range.");
            return;
        }
        m_draft.eventSeed = seed;
        m_draft.defaultPreset = static_cast<Preset>(m_preset);
        if (app.Check(ApiSetSettings(m_draft))) app.Go(ScreenId::Menu);
    }
}

void SettingsScreen::Draw(App& app)
{
    Rectangle c = app.Content();
    Ui::Label(c.x, c.y + 5, "Race configuration", 30);
    Ui::BeginClip({ c.x, c.y + 60, c.width, c.height - 130 });
    float x = c.x + 180;
    float y = c.y + 76 - m_settingsScroll;
    Ui::Number({ x, y, 220, 36 }, "Laps", &m_draft.laps, 2, 3);
    Ui::Number({ x, y + 56, 220, 36 }, "Opponents", &m_draft.opponentCount, 3, 9);
    Ui::Number({ x, y + 112, 220, 36 }, "Preset 0..3", &m_preset, 0, 3);
    Ui::Label(x + 240, y + 120, Format::PresetName(static_cast<Preset>(m_preset)), 18, Ui::Accent);
    Ui::Number({ x, y + 168, 220, 36 }, "Starting nitro", &m_draft.startNitroCharges, 0, 3);
    Ui::Label(c.x, y + 232, "Reserved seed", 18);
    Ui::TextBox({ x, y + 224, 260, 38 }, m_seed, sizeof(m_seed), &m_seedEdit);
    Ui::Button(3, { c.x, y + 286, 440, 36 }, m_draft.showCostLabels ? "Map scalar-cost labels: ON" : "Map scalar-cost labels: OFF");
    Ui::Wrap({ c.x, y + 346, c.width, 100 }, "Events follow the authored circuit script; the stored seed is reserved for future event generation. Settings apply to the next race and are locked during an active session.");
    Ui::EndClip();
    Ui::Button(1, { c.x, c.y + c.height - 48, 180, 42 }, "Cancel");
    Ui::Button(2, { c.x + 200, c.y + c.height - 48, 220, 42 }, "Save configuration", !app.SessionLocked());
}

void CircuitSelectScreen::Update(App& app, float)
{
    int action = Ui::TakeAction();
    if (action == 1) app.Go(ScreenId::Menu);
    if (action >= 10 && action < 10 + ApiCircuitCount() && !app.SessionLocked())
    {
        if (app.LoadCircuit(action - 10)) app.Go(ScreenId::Explorer);
    }
}

void CircuitSelectScreen::Draw(App& app)
{
    Rectangle c = app.Content();
    Ui::Button(1, { c.x, c.y, 140, 38 }, "Back");
    Ui::Label(c.x, c.y + 65, "Choose your circuit", 32);
    for (int i = 0; i < ApiCircuitCount(); ++i)
    {
        char label[100];
        snprintf(label, sizeof(label), "%02d   /   %s", i + 1, ApiCircuitName(i));
        Ui::Button(10 + i, { c.x, c.y + 135 + i * 100, c.width, 76 }, label, !app.SessionLocked());
    }
}
