#pragma once

#include "AppState.h"
#include "ui/Toast.h"
#include "raylib.h"

class App
{
public:
    AppState state;
    Toast toast;
    void Init();
    void Shutdown();
    void Update(float dt);
    void Draw();
    void Go(ScreenId id);
    ScreenId Current() const
    {
        return m_current;
    }
    Rectangle Content() const;
    bool Check(ApiStatus status);
    bool LoadCircuit(int index);
    bool StartRace();
    void CaptureResults();
    bool SessionLocked() const;
    bool Available(ScreenId id) const;
private:
    Screen* m_screens[static_cast<int>(ScreenId::Count)] = {};
    ScreenId m_current = ScreenId::Menu;
};
