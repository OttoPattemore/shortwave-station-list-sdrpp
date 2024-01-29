#pragma once
#include <string>
struct Station
{
    std::string name;
    std::string notes;
    int power;
    int frequency;
    float lat;
    float lon;
    int utcMin;
    int utcMax;
};
