#pragma once

#include "Screen.h"
#include "api/SmartRaceApi.h"

class MainMenuScreen : public Screen
{
public:
    void Update(App&, float) override;
    void Draw(App&) override;
};

class InstructionsScreen : public Screen
{
public:
    void Update(App&, float) override;
    void Draw(App&) override;
private:
    float m_scroll = 0;
};

class SettingsScreen : public Screen
{
public:
    void Enter(App&) override;
    void Update(App&, float) override;
    void Draw(App&) override;
private:
    Settings m_draft = {};
    int m_preset = 0;
    char m_seed[24] = {};
    bool m_seedEdit = false;
    float m_settingsScroll = 0;
};

class CircuitSelectScreen : public Screen
{
public:
    void Update(App&, float) override;
    void Draw(App&) override;
};
