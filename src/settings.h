#pragma once

struct Settings
{
    bool useLocalHost = false;
    bool showStations = true;

    bool calculateDistances = false;
    float lat = 0;
    float lon = 0;
};
void initSettings();
Settings loadSettings();
void saveSettings(const Settings& settings);