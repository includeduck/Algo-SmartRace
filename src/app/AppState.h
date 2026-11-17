#pragma once

#include "api/SmartRaceApi.h"
#include "Screen.h"

struct AppState
{
    bool championship = false;
    bool raceStarted = false;
    bool raceRecorded = false;
    bool dataReady = false;
    bool quit = false;
    int selectedCircuit = 0;
    int raceSerial = 0;
    Preset activePreset = Preset::Balanced;
    ScreenId labReturn = ScreenId::Menu;
    char racerNames[10][48] = {};
    ResultRowView resultRows[10] = {};
    int resultCount = 0;
};
