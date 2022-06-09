#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <module.h>
#include <gui/gui.h>

#include <fstream>
#include <iostream>
#include <json.hpp>
#include <config.h>
#include "utc.h"
#include "distance.h"
#include "remotes.h"
#include "settings.h"
#include "gui/widgets/file_select.h"
SDRPP_MOD_INFO{
    /* Name:            */ "Shortwave Staton List",
    /* Description:     */ "Plugin to show data from shortwave-station-list in SDR++",
    /* Author:          */ "Otto Pattemore",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1};


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
    ShortwaveStationList(std::string name) : m_LocalSourceLocation("")
    {
        initSettings();
        // GUI callbacks
        gui::menu.registerEntry(name, menuHandler, this, NULL);
        fftRedrawHandler.ctx = this;
        fftRedrawHandler.handler = fftRedraw;
        gui::waterfall.onFFTRedraw.bindHandler(&fftRedrawHandler);

        // Init
        
        settings = loadSettings();
        reloadSource();
    }
    void reloadSource()
    {
        if(settings.useLocalSource && std::filesystem::exists(settings.localSourceFile))
        {
            source = new LocalSource(settings.localSourceFile);
        }
        else
        {
            source = new RemoteSource("https://ottopattemore.github.io/shortwave-station-list/db/eibi.json");
        }
    }
    ~ShortwaveStationList()
    {
        delete source;
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
        if(ImGui::Checkbox("Use Local Source", &_this->settings.useLocalSource))
        {
            _this->reloadSource();
        }
        if(_this->settings.useLocalSource)
        {
            if(_this->m_LocalSourceLocation.render("##localsource_selector"))
            {
                if(_this->m_LocalSourceLocation.pathIsValid())
                {
                    _this->settings.localSourceFile = _this->m_LocalSourceLocation.path;
                    saveSettings(_this->settings);
                    _this->reloadSource();
                }
            }
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
        if(ImGui::TreeNode("Options")){
            if(ImGui::ColorEdit3("Marker color", &_this->settings.displayR))
            {
                saveSettings(_this->settings);
            }
            if(ImGui::SliderFloat("Marker Alpha", &_this->settings.displayA,0,1))
            {
                saveSettings(_this->settings);
            }
            if(ImGui::ColorEdit3("Marker text color", &_this->settings.markerTextColorR))
            {
                saveSettings(_this->settings);
            }
            if(ImGui::SliderFloat("Marker text Alpha", &_this->settings.markerTextColorA, 0, 1))
            {
                saveSettings(_this->settings);
            }
            ImGui::TreePop();
        }

    }
    static void fftRedraw(ImGui::WaterFall::FFTRedrawArgs args, void *ctx)
    {
        ShortwaveStationList *_this = (ShortwaveStationList *)ctx;

        // Don't show stations
        if (!_this->settings.showStations) return;
        // Handler for display FFT marker
        std::function<void(std::string name, int freq, std::function<void()> tooltip)> draw = [_this,&args](std::string name, int freq, std::function<void()> tooltip)
        {
            double centerXpos = args.min.x + std::round((freq - args.lowFreq) * args.freqToPixelRatio);

            if (freq >= args.lowFreq && freq <= args.highFreq)
            {
                const ImVec4 bgColor = ImVec4(_this->settings.displayR, _this->settings.displayG, _this->settings.displayB, _this->settings.displayA);
                args.window->DrawList->AddLine(ImVec2(centerXpos, args.min.y), ImVec2(centerXpos, args.max.y), ImGui::ColorConvertFloat4ToU32(bgColor));
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
                const ImVec4 bgColor = ImVec4(_this->settings.displayR,_this->settings.displayG,_this->settings.displayB,_this->settings.displayA);
                args.window->DrawList->AddRectFilled(clampedRectMin, clampedRectMax, ImGui::ColorConvertFloat4ToU32(bgColor));
            }
            if (rectMin.x >= args.min.x && rectMax.x <= args.max.x)
            {
                const ImVec4 textColor = ImVec4(_this->settings.markerTextColorR, _this->settings.markerTextColorB, _this->settings.markerTextColorB, _this->settings.markerTextColorA);
                args.window->DrawList->AddText(ImVec2(centerXpos - (nameSize.x / 2), args.min.y), ImGui::ColorConvertFloat4ToU32(textColor), name.c_str());
            }
        };
        
        
        for(const auto& f : _this->source->getStations()){
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
    std::string name;
    bool enabled = true;
    EventHandler<ImGui::WaterFall::FFTRedrawArgs> fftRedrawHandler;
    DataSource* source = nullptr;
    FileSelect m_LocalSourceLocation;
    public:
        Settings settings;
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