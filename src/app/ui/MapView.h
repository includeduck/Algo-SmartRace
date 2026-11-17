#pragma once

#include "raylib.h"
#include "api/SmartRaceApi.h"

class MapView
{
public:
    Rectangle bounds = {};
    void Fit(GraphView graph);
    void Update();
    Vector2 Point(double x, double y) const;
    int HitVertex(GraphView graph) const;
    int HitEdge(GraphView graph) const;
    void Draw(GraphView graph, ArrayView<int> edges = {}, ArrayView<int> vertices = {}, const LabFrame* frame = nullptr, bool generator = false) const;
    void Cars(RaceSnapshot snapshot) const;
private:
    Vector2 m_center = {};
    Vector2 m_pan = {};
    float m_spanX = 1;
    float m_spanY = 1;
    float m_zoom = 1;
    float Scale() const;
};
