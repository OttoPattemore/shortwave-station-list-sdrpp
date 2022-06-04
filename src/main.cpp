#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <module.h>
#include <gui/gui.h>

#include <fstream>
#include <curl/curl.h>
#include <iostream>
#include <json.hpp>
#include <config.h>
#include "utc.h"
#include "distance.h"
#include "remotes.h"
#include "settings.h"
SDRPP_MOD_INFO{
    /* Name:            */ "Shortwave Staton List",
    /* Description:     */ "Plugin to show data from shortwave-station-list in SDR++",
    /* Author:          */ "Otto Pattemore",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1};

struct Station{
    std::string name;
    std::string notes;
    int power;
    int frequency;
    float lat;
    float lon;
    int utcMin;
    int utcMax;
};

bool isStationLive(Station station, int time = getUTCTime())
{
    /*
        NOTE: This will crash and burn if there are different broadcast times per week day.
    */

    if (station.utcMin < station.utcMax)
    {
        // This station does not cross 24-hour time.
        return ((time > station.utcMin) && (time < station.utcMax));
    }
    else
    {

        /*
            We'll split the station into two.
            Then check each one individually.
        */

        // Split it into two broadcasts
        Station firstDay = station;
        firstDay.utcMax = 24000;
        Station secondDay = station;
        secondDay.utcMin = 0;

        // Check each individually
        return isStationLive(firstDay, time) || isStationLive(secondDay, time);
    }
}


class ShortwaveStationList : public ModuleManager::Instance
{
public:
    ShortwaveStationList(std::string name)
    {
        initSettings();
        // GUI callbacks
        gui::menu.registerEntry(name, menuHandler, this, NULL);
        fftRedrawHandler.ctx = this;
        fftRedrawHandler.handler = fftRedraw;
        gui::waterfall.onFFTRedraw.bindHandler(&fftRedrawHandler);

        // Init
        
        settings = loadSettings();
        loadDatabase();
    }
    void loadDatabase()
    {
        stations.clear();

        // Get remote URL
        const std::string url = getRemoteURL(settings.useLocalHost);

        // Fetch from remote
        loadList(url + "/db/eibi.json");


    }
    ~ShortwaveStationList()
    {
    }

    void postInit() {}

    void enable()
    {
    }

    void disable()
    {
    }

    bool isEnabled()
    {
        return enabled;
    }

private:
    void loadList(std::string url){
        std::string list;

        CURL *curl = curl_easy_init();
        if (!curl)
        {
            spdlog::error("[ Shortwave Station List ] Failed to init curl!");
            return;
        }
        // Download data
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &list);
        size_t (*handler)(char *buffer, size_t itemSize, size_t nitems, void *ctx) = [](char *buffer, size_t itemSize, size_t nitems, void *ctx) -> size_t
        {
            size_t bytes = itemSize * nitems;
            std::string data;
            data.resize(bytes);
            memcpy(data.data(), buffer,bytes);
            ((std::string*)ctx)->append(data);
            return bytes;
        };
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handler);
        CURLcode result  = curl_easy_perform(curl);
        // Handle errors
        if(result != CURLE_OK){
            errorLoading = true;
            spdlog::error("[ Shortwave Station List ] Failed to load {}",url);
        }

        // Once the list is download decode the json
        nlohmann::json database = nullptr;
        try{
            database = json::parse(list);
        }catch(nlohmann::json::parse_error){
            errorLoading = true;
        }
        for( const auto station : database["stations"] ){
            Station s;
            s.frequency = station["frequency"].get<int>();
            s.name = station["name"];
            s.power = station["power"].get<int>();
            s.notes = station["notes"];
            s.lat = station["location"][0].get<float>();
            s.lon = station["location"][1].get<float>();
            s.utcMin = std::stof(station["utc_start"].get<std::string>());
            s.utcMax = std::stof(station["utc_end"].get<std::string>());
            stations[s.frequency].push_back(s);
        }
        spdlog::info("Fetch list: {}", url);
        // Cleanup
        curl_easy_cleanup(curl);
    }
    static void menuHandler(void *ctx)
    {
        ShortwaveStationList * _this = (ShortwaveStationList *)ctx;
        if(ImGui::IsKeyPressed(ImGuiKey_RightShift, false))
        {
            _this->settings.showStations = !_this->settings.showStations;
            saveSettings(_this->settings);
        }
        ImGui::TextColored(ImVec4(0.5,0.9,0.5,1),"Current UTC Time: %02i:%02i", getUTCHour(),getUTCMin());
        if(ImGui::Checkbox("Display on FFT", &_this->settings.showStations))
        {
            saveSettings(_this->settings);
        }
        if(ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("Use Right Shift to toggle");
            ImGui::EndTooltip();
        }
        if (ImGui::BeginCombo("Source", _this->settings.useLocalHost ? "Local Host ( For testing )" : "Remote ( Default )"))
        {
            if (ImGui::Selectable("Remote ( Default )"))
            {
                _this->settings.useLocalHost = false;
                saveSettings(_this->settings);
                _this->loadDatabase();
            }
            if (ImGui::Selectable("Local Host ( For testing )"))
            {
                _this->settings.useLocalHost = true;
                saveSettings(_this->settings);
                _this->loadDatabase();
            }
            ImGui::EndCombo();
        }
        if(ImGui::TreeNode("Location"))
        {
            ImGui::Checkbox("Calculate distances to station", &_this->settings.calculateDistances);
            if(!_this->settings.calculateDistances)
            {
                style::beginDisabled();
            }

            if(ImGui::InputFloat("Lat", &_this->settings.lat))
            {
                saveSettings(_this->settings);
            }
            if(ImGui::InputFloat("Lon", &_this->settings.lon))
            {
                saveSettings(_this->settings);
            }
            if (!_this->settings.calculateDistances)
            {
                style::endDisabled();
            }
            ImGui::TreePop();
        }
        if (_this->errorLoading)
        {
            ImGui::OpenPopup("Failed to download database!");
        }
        ImGui::SetNextWindowSize(ImVec2(600,200));
        if (ImGui::BeginPopupModal("Failed to download database!", &_this->errorLoading, ImGuiWindowFlags_NoMove))
        {
            ImGui::TextColored(ImVec4(1, 0.2, 0.2, 1) ,"Failed to download database!");
            ImGui::Text("1. Check your network connection");
            ImGui::Text("2. Check you are using an up to date plugin");
            if(ImGui::Button("Close", ImVec2(ImGui::GetContentRegionAvail().x,0)))
            {
                _this->errorLoading = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        /*if (ImGui::Button("Search for station##button"))
        {
            ImGui::OpenPopup("Search for station");
        }
        ImGui::SetNextWindowSize(ImVec2(800,600));
        if(ImGui::BeginPopupModal("Search for station",nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)){
            static char searchTerm[100];
            ImGui::InputText("Search", searchTerm,100);
            if(ImGui::BeginTable("stations",2))
            {
                for (const auto &frequency : _this->stations)
                {
                    for (const auto &station : frequency.second)
                    {
                        std::string lowerName = station.name;
                        std::string lowerSearch = searchTerm;

                        // Convert to lower
                        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),[](unsigned char c) { return std::tolower(c); });

                        std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), [](unsigned char c) { return std::tolower(c); });

                        if (lowerName.find(lowerSearch) == std::string::npos) continue;
                        if(!isStationLive(station)) continue;
                        ImGui::TableNextRow();
                        ImGui::Text("%s",station.name.c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%ikHz",station.frequency);
                        ImGui::TableNextColumn();
                    }
                }
                ImGui::EndTabBar();
            }
            if ( ImGui::Button("Close") ){
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }*/
    }
    static void fftRedraw(ImGui::WaterFall::FFTRedrawArgs args, void *ctx)
    {
        ShortwaveStationList *_this = (ShortwaveStationList *)ctx;

        // Don't show stations
        if (!_this->settings.showStations) return;

        // Handler for display FFT marker
        std::function<void(std::string name, int freq, std::function<void()> tooltip)> draw = [&args](std::string name, int freq, std::function<void()> tooltip)
        {
            double centerXpos = args.min.x + std::round((freq - args.lowFreq) * args.freqToPixelRatio);

            if (freq >= args.lowFreq && freq <= args.highFreq)
            {
                args.window->DrawList->AddLine(ImVec2(centerXpos, args.min.y), ImVec2(centerXpos, args.max.y), IM_COL32(232, 93, 0, 255));
            }

            ImVec2 nameSize = ImGui::CalcTextSize(name.c_str());
            ImVec2 rectMin = ImVec2(centerXpos - (nameSize.x / 2) - 5, args.min.y);
            ImVec2 rectMax = ImVec2(centerXpos + (nameSize.x / 2) + 5, args.min.y + nameSize.y);
            ImVec2 clampedRectMin = ImVec2(std::clamp<double>(rectMin.x, args.min.x, args.max.x), rectMin.y);
            ImVec2 clampedRectMax = ImVec2(std::clamp<double>(rectMax.x, args.min.x, args.max.x), rectMax.y);
            if (ImGui::IsMouseHoveringRect(clampedRectMin, clampedRectMax))
            {
                ImGui::BeginTooltip();
                tooltip();
                ImGui::EndTooltip();
                if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    if (gui::waterfall.selectedVFO == "")
                    {
                        // TODO: Replace with proper tune call
                        gui::waterfall.setCenterFrequency(freq);
                        gui::waterfall.centerFreqMoved = true;
                    }
                    else
                    {
                        tuner::tune(tuner::TUNER_MODE_NORMAL, gui::waterfall.selectedVFO, freq);
                    }
                }
            }
            if (clampedRectMax.x - clampedRectMin.x > 0)
            {
                args.window->DrawList->AddRectFilled(clampedRectMin, clampedRectMax, IM_COL32(232, 93, 0,255));
            }
            if (rectMin.x >= args.min.x && rectMax.x <= args.max.x)
            {
                args.window->DrawList->AddText(ImVec2(centerXpos - (nameSize.x / 2), args.min.y), IM_COL32(0, 0, 0, 255), name.c_str());
            }
        };
        
        
        for(const auto& f : _this->stations){
            const auto frequency = f.first;
            auto stations = f.second;

            //TODO: This isn't a great way of doing it
            std::vector<Station> liveStations;

            for(const auto station : stations){

                // Check time
                if (isStationLive(station))
                {
                    liveStations.push_back(station);
                }
            }
            std::sort(liveStations.begin(), liveStations.end(), [_this](Station a, Station b)
                      { return distanceEarth(a.lat, a.lon, _this->settings.lat, _this->settings.lon) < distanceEarth(b.lat, b.lon, _this->settings.lat, _this->settings.lon); });
            // There are no live stations
            if(!liveStations.size()) continue;

            const std::string text = liveStations[0].name;
            draw(text, frequency * 1000, [&liveStations,_this,frequency]()
                {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.5,0.5,1,1), "Live stations on %ikHz:", frequency);
                    for (const auto station : liveStations)
                    {
                        ImGui::Text("%s", station.name.c_str());
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.5, 0.9, 0.5, 1), "[%i - %i]", station.utcMin, station.utcMax);
                        bool hasLocation = ((station.lat != 0) || (station.lon != 0));
                        if (_this->settings.calculateDistances && hasLocation)
                        {
                            ImGui::SameLine();
                            ImGui::TextColored(ImVec4(0.8, 0.5, 0.5, 1), "%ikm", (int)distanceEarth(station.lat,station.lon,_this->settings.lat,_this->settings.lon));
                        }
                    }
                    ImGui::Separator();
                }
            );
        }
    }
    
    Settings settings;

    std::string name;
    bool enabled = true;
    EventHandler<ImGui::WaterFall::FFTRedrawArgs> fftRedrawHandler;
    std::unordered_map<int, std::vector<Station>> stations;
    bool errorLoading = false;
};
void runTests()
{
    const std::function<void(std::string, std::function<bool()>)> CHECK = [](std::string name, std::function<bool()> test)
    {
        if(test())
        {
            spdlog::info("[ Shortwave Station List ] ✅ {} passed!", name);
        }
        else
        {
            spdlog::error("[ Shortwave Station List ] ❌ {} failed!");
        }
    };
    CHECK("UTC Same day", [](){
        Station testStation;
        testStation.utcMin = 1000;
        testStation.utcMax = 5000;
        return isStationLive(testStation,4000) && (!isStationLive(testStation,6000));
    });
    CHECK("UTC Multiple day", [](){
        Station testStation;
        testStation.utcMin = 1800;
        testStation.utcMax = 500;
        return isStationLive(testStation, 400) && isStationLive(testStation, 1900) && (!isStationLive(testStation,1700)) && (!isStationLive(testStation,600)); 
    });
}
MOD_EXPORT void _INIT_()
{
    runTests();
}

MOD_EXPORT ModuleManager::Instance *_CREATE_INSTANCE_(std::string name)
{
    return new ShortwaveStationList(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void *instance)
{
    delete (ShortwaveStationList *)instance;
}

MOD_EXPORT void _END_()
{
}