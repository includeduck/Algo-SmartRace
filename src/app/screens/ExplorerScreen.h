#pragma once

#include "Screen.h"
#include "ui/MapView.h"
#include "ui/OwnedArray.h"

class ExplorerScreen : public Screen
{
public:
    void Enter(App&) override;
    void Update(App&, float) override;
    void Draw(App&) override;
private:
    MapView m_map;
    GraphView m_graph = {};
    TrackInfo m_info = {};
    ValidationView m_validation = {};
    ReachabilityView m_bfs = {};
    ComponentsView m_components = {};
    CycleView m_cycle = {};
    DeadEndsView m_deadEnds = {};
    RouteView m_route = {};
    RouteCompareView m_alternatives = {};
    RouteCompareView m_raceAlternatives = {};
    ArrayView<double> m_costs = {};
    PickupView m_pickups = {};
    RefuelView m_refuel = {};
    OwnedArray<int> m_routeEdges;
    OwnedArray<int> m_routeVertices;
    int m_tab = 0;
    int m_source = 0;
    int m_target = -1;
    int m_preset = 0;
    int m_algorithm = 0;
    int m_budget = 150;
    int m_selectedVertex = -1;
    int m_selectedEdge = -1;
    int m_candidate = -1;
    bool m_raceCandidate = false;
    bool m_live = false;
    int m_routeScope = 0;
    int m_routeTarget = 0;
    int m_lastInputs[7] = {};
    long long m_comparisonExpanded = 0;
    char m_routeError[256] = {};
    bool m_dirty = true;
    double m_comparisonCost = 0;
    bool m_comparisonFound = false;
    float m_scroll = 0;
    float m_detailHeight = 0;
    Rectangle m_details = {};
    void Refresh();
    void Details(App&, float x, float& y, float width);
};
