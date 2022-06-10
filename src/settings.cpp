#include "settings.h"
#include <config.h>
#include "core.h"

ConfigManager config;
void initSettings()
{
    json def = json({});
    config.setPath(core::args["root"].s() + "/shortwave_station_list.json");
    config.load(def);
    config.enableAutoSave();
}
Settings loadSettings()
{
    Settings loadedSettings;
    config.acquire();
    if (config.conf.contains("calculateDistances"))
        loadedSettings.calculateDistances = config.conf["calculateDistances"].get<bool>();
    if (config.conf.contains("lat"))
        loadedSettings.lat = config.conf["lat"].get<float>();
    if (config.conf.contains("lon"))
        loadedSettings.lon = config.conf["lon"].get<float>();
    if (config.conf.contains("showStations"))
        loadedSettings.showStations = config.conf["showStations"].get<bool>();
    if(config.conf.contains("useLocalSource"))
        loadedSettings.showStations = config.conf["useLocalSource"].get<bool>();
    if (config.conf.contains("markerBG_R"))
        loadedSettings.displayR = config.conf["markerBG_R"].get<float>();
    if (config.conf.contains("markerBG_G"))
        loadedSettings.displayG = config.conf["markerBG_G"].get<float>();
    if (config.conf.contains("markerBG_B"))
        loadedSettings.displayB = config.conf["markerBG_B"].get<float>();

    if (config.conf.contains("markerTextColor_R"))
        loadedSettings.markerTextColorR = config.conf["markerTextColor_R"].get<float>();
    if (config.conf.contains("markerTextColor_G"))
        loadedSettings.markerTextColorG = config.conf["markerTextColor_G"].get<float>();
    if (config.conf.contains("markerTextColor_B"))
        loadedSettings.markerTextColorB = config.conf["markerTextColor_B"].get<float>();
    if(config.conf.contains("localSourceFile"))
        loadedSettings.localSourceFile =config.conf["localSourceFile"].get<std::string>();
    if(config.conf.contains("fadeWhenZoomed"))
        loadedSettings.showStations = config.conf["fadeWhenZoomed"].get<bool>();
    config.release(true);
    return loadedSettings;
}
void saveSettings(const Settings &settings)
{
    config.acquire();
    config.conf["calculateDistances"] = settings.calculateDistances;
    config.conf["lat"] = settings.lat;
    config.conf["lon"] = settings.lon;
    config.conf["showStations"] = settings.showStations;
    config.conf["useLocalSource"] = settings.useLocalSource;
    config.conf["markerBG_R"] = settings.displayR;
    config.conf["markerBG_G"] = settings.displayG;
    config.conf["markerBG_B"] = settings.displayB;
    config.conf["markerTextColor_R"] = settings.markerTextColorR;
    config.conf["markerTextColor_G"] = settings.markerTextColorG;
    config.conf["markerTextColor_B"] = settings.markerTextColorB;
    config.conf["localSourceFile"] = settings.localSourceFile;
    config.conf["fadeWhenZoomed"] = settings.fadeWhenZoomed;
    config.release(true);
    config.save();
}