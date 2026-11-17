#pragma once
#include "Screen.h"
#include "api/SmartRaceApi.h"
#include "raylib.h"

class ChampionshipScreen : public Screen
{
public:
    void Enter(App&) override;
    void Update(App&, float) override;
    void Draw(App&) override;
private:
    ChampionshipStatus m_status = {};
    StandingsView m_standings = {};
    TopKView m_top = {};
    ResultsView m_history = {};
    int m_method = 0;
    int m_k = 3;
    int m_historyIndex = 0;
    int m_racerA = 0;
    int m_racerB = 1;
    TieBreak m_reason = TieBreak::SameRacer;
    bool m_dirty = true;
    int m_lastK = -1;
    int m_lastHistory = -1;
    int m_page = 0;
    float m_scroll = 0;
    float m_height = 0;
    Rectangle m_area = {};
};
