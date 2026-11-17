#pragma once

class App;

enum class ScreenId
{
    Menu, Instructions, Settings, Circuits, Explorer, Race, Results, Championship, Lab, Count
};

class Screen
{
public:
    virtual ~Screen() = default;
    virtual void Enter(App&)
    {
    }
    virtual void Update(App&, float) = 0;
    virtual void Draw(App&) = 0;
};
