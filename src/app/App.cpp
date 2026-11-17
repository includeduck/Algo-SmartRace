#include "App.h"
#include "ui/Theme.h"
#include "screens/MenuScreens.h"
#include "screens/ExplorerScreen.h"
#include "screens/RaceScreen.h"
#include "screens/ChampionshipScreen.h"
#include "screens/LabScreen.h"
#include <cstdio>

void App::Init()
{
    m_screens[static_cast<int>(ScreenId::Menu)] = new MainMenuScreen();
    m_screens[static_cast<int>(ScreenId::Instructions)] = new InstructionsScreen();
    m_screens[static_cast<int>(ScreenId::Settings)] = new SettingsScreen();
    m_screens[static_cast<int>(ScreenId::Circuits)] = new CircuitSelectScreen();
    m_screens[static_cast<int>(ScreenId::Explorer)] = new ExplorerScreen();
    m_screens[static_cast<int>(ScreenId::Race)] = new RaceScreen();
    m_screens[static_cast<int>(ScreenId::Results)] = new ResultsScreen();
    m_screens[static_cast<int>(ScreenId::Championship)] = new ChampionshipScreen();
    m_screens[static_cast<int>(ScreenId::Lab)] = new LabScreen();
    if (Check(ApiInit("data"))) LoadCircuit(0);
    Go(ScreenId::Menu);
}

void App::Shutdown()
{
    for (int i = 0; i < static_cast<int>(ScreenId::Count); ++i) delete m_screens[i];
    ApiShutdown();
}

bool App::Available(ScreenId id) const
{
    return m_screens[static_cast<int>(id)] != nullptr;
}

bool App::SessionLocked() const
{
    ChampionshipStatus status = ApiChampionshipGetStatus();
    return (state.championship && status.active && !status.complete) || (state.raceStarted && !ApiRaceOver());
}

bool App::Check(ApiStatus status)
{
    if (status == ApiStatus::Ok) return true;
    toast.Show(ApiLastError());
    return false;
}

bool App::LoadCircuit(int index)
{
    if (!Check(ApiLoadCircuit(index)))
    {
        state.dataReady = false;
        state.raceStarted = false;
        toast.Show("Circuit data could not load. Run beside the copied data/ folder or from the repository root.", true);
        return false;
    }
    state.selectedCircuit = index;
    toast.Clear();
    state.dataReady = true;
    state.raceStarted = false;
    return true;
}

bool App::StartRace()
{
    if (!Check(ApiRaceStart())) return false;
    state.raceStarted = true;
    state.raceRecorded = false;
    state.activePreset = ApiGetSettings().defaultPreset;
    ++state.raceSerial;
    RaceSnapshot snapshot = ApiRaceSnapshot();
    for (int i = 0; i < snapshot.racers.count; ++i)
    {
        const RacerView& r = snapshot.racers[i];
        if (r.id >= 0 && r.id < 10) snprintf(state.racerNames[r.id], 48, "%s", r.name);
    }
    Go(ScreenId::Race);
    return true;
}

void App::CaptureResults()
{
    ResultsView results = ApiRaceResults();
    state.resultCount = results.rows.count < 10 ? results.rows.count : 10;
    for (int i = 0; i < state.resultCount; ++i) state.resultRows[i] = results.rows[i];
}

void App::Go(ScreenId id)
{
    if (!Available(id)) return;
    m_current = id;
    Ui::ClearAction();
    m_screens[static_cast<int>(id)]->Enter(*this);
    // Navigation can invalidate backend views (e.g. loading the next circuit).
    // Refresh the destination before its first Draw in this same frame.
    m_screens[static_cast<int>(id)]->Update(*this, 0);
}

Rectangle App::Content() const
{
    return { 28, 110, static_cast<float>(GetScreenWidth() - 56), static_cast<float>(GetScreenHeight() - 160) };
}

void App::Update(float dt)
{
    toast.Update(dt);
    m_screens[static_cast<int>(m_current)]->Update(*this, dt);
}

void App::Draw()
{
    Ui::Begin();
    DrawRectangle(0, 0, GetScreenWidth(), 84, Ui::Panel);
    Ui::Label(28, 21, "SMART / RACE", 28, Ui::Accent);
    Ui::Label(28, 56, "THE ALGORITHMIC RACING CHAMPIONSHIP", 13, Ui::Muted);
    const char* titles[] = { "Paddock", "Driver handbook", "Race configuration", "Choose a circuit", "Track explorer", "Live race", "Race results", "Championship", "Algorithm lab" };
    Ui::Label(static_cast<float>(GetScreenWidth() - 270), 32, titles[static_cast<int>(m_current)], 20);
    m_screens[static_cast<int>(m_current)]->Draw(*this);
    char footer[160];
    snprintf(footer, sizeof(footer), "OFFLINE  /  DETERMINISTIC  /  API v%d.%d       TAB focus  |  ENTER activate", SMARTRACE_API_VERSION / 10, SMARTRACE_API_VERSION % 10);
    Ui::Label(28, static_cast<float>(GetScreenHeight() - 25), footer, 14, Ui::Muted);
    toast.Draw();
    Ui::End();
}
