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
    if (config.conf.contains("useLocalHost"))
        loadedSettings.useLocalHost = config.conf["useLocalHost"].get<bool>();
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
    config.conf["useLocalHost"] = settings.useLocalHost;
    config.release(true);
    config.save();
}