#pragma once

struct Settings
{
    bool useLocalHost = false;
    bool showStations = true;

    bool calculateDistances = false;
    float lat = 0;
    float lon = 0;
    float displayR = 0.5;
    float displayG = 0.5;
    float displayB = 0.7;
    float displayA = 255;
    float markerTextColorR = 0.5;
    float markerTextColorG = 0.5;
    float markerTextColorB = 0.7;
    float markerTextColorA = 255;
};
void initSettings();
Settings loadSettings();
void saveSettings(const Settings& settings);