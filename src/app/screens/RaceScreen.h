#pragma once
#include "Screen.h"
#include "ui/MapView.h"
#include "ui/RaceClock.h"

class RaceScreen : public Screen
{
public:
    void Enter(App&) override;
    void Update(App&, float) override;
    void Draw(App&) override;
private:
    int m_serial = -1;
    bool m_paused = false;
    bool m_autopilot = true;
    bool m_refuelPopup = false;
    bool m_choiceQueued = false;
    bool m_waiting = false;
    int m_promptVertex = -1;
    int m_purchase = 0;
    int m_speed = 1;
    int m_lastLog = -1;
    int m_logCount = 0;
    int m_logHead = 0;
    int m_logOffset = 0;
    char m_log[200][256] = {};
    RaceClock m_clock;
    MapView m_map;
    GraphView m_graph = {};
    RaceSnapshot m_snapshot = {};
    RacerView m_human = {};
    ResultsView m_order = {};
    Rectangle m_details = {};
    float m_scroll = 0;
    float m_detailHeight = 0;
    void Pull();
    void DecisionPoint();
    bool NeedsDecision() const;
    void Logs();
};

class ResultsScreen : public Screen
{
public:
    void Update(App&, float) override;
    void Draw(App&) override;
private:
    float m_scroll = 0;
};
