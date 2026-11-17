#pragma once
#include "Screen.h"
#include "ui/MapView.h"

class LabScreen : public Screen
{
public:
    void Enter(App&) override;
    void Update(App&, float) override;
    void Draw(App&) override;
private:
    int m_module = 0;
    int m_displayedModule = -1;
    int m_step = -1;
    int m_rate = 10;
    int m_seed = 2;
    int m_vertices = 28;
    int m_preset = 0;
    int m_tableRow = 0;
    int m_tableColumn = 0;
    int m_eventOffset = 0;
    float m_scrub = -1;
    float m_scroll = 0;
    float m_detailHeight = 0;
    float m_codeScroll = 0;
    float m_playTime = 0;
    bool m_ready = false;
    bool m_playing = false;
    LabInput m_input = {};
    LabFrame m_frame = {};
    GraphView m_graph = {};
    PseudocodeView m_code = {};
    ArrayView<LabEventView> m_events = {};
    GeneratorReportView m_generator = {};
    char m_result[512] = {};
    LabRunInfo m_runInfo = {};
    char m_sourceDescription[512] = {};
    RouteView m_endpointSummary = {};
    bool m_hasEndpointSummary = false;
    MapView m_map;
    Rectangle m_details = {};
    Rectangle m_visual = {};
    Rectangle m_pseudocode = {};
    void Run(App&);
    void Inputs(App&, float x, float& y, float width);
};
