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
};
class ShortwaveStationList : public ModuleManager::Instance
{
public:
    ShortwaveStationList(std::string name)
    {
        gui::menu.registerEntry(name, menuHandler, this, NULL);

        fftRedrawHandler.ctx = this;
        fftRedrawHandler.handler = fftRedraw;
        gui::waterfall.onFFTRedraw.bindHandler(&fftRedrawHandler);

        
        CURL* curl = curl_easy_init();

        if(!curl){
            spdlog::error("[ Shortwave Station List ] Failed to init curl!");
            return;
        }

        // Download data
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8000/sw.json");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ShortwaveStationList::handleData);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
        CURLcode result  = curl_easy_perform(curl);

        // Handle errors
        if(result != CURLE_OK){
            spdlog::error("[ Shortwave Station List ] Failed to download list!");
        }

        // Once the list is download decode the json
        const auto database = json::parse(downloadedData);
        for( const auto station : database["stations"] ){
            Station s;
            s.frequency = station["frequency"].get<int>();
            s.name = station["name"];
            s.power = station["power"].get<int>();
            s.notes = station["notes"];
            s.lat = station["location"][0].get<float>();
            s.lon = station["location"][1].get<float>();
            stations[s.frequency].push_back(s);
        }

        // Cleanup
        curl_easy_cleanup(curl);
        spdlog::info("[ Shortwave Station List ] Downloaded station list!");
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
    static size_t handleData(char *buffer, size_t itemSize, size_t nitems, void * ctx)
    {
        ShortwaveStationList *_this = (ShortwaveStationList *)ctx;
        size_t bytes = itemSize * nitems;
        std::string data;
        data.resize(bytes);
        memcpy(data.data(), buffer,bytes);
        _this->downloadedData.append(data);
        return bytes;
    }
    static void menuHandler(void *ctx)
    {
        ShortwaveStationList * _this = (ShortwaveStationList *)ctx;
    }
    static void fftRedraw(ImGui::WaterFall::FFTRedrawArgs args, void *ctx)
    {
        ShortwaveStationList *_this = (ShortwaveStationList *)ctx;
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
            const auto stations = f.second;
            // Closest station should be displayed
            std::string text = stations[0].name;

            if( stations.size() > 1 ){
                text += " and " + std::to_string(stations.size() - 1) + " others";
            }
            draw(text, frequency*1000,[stations](){
                ImGui::Text("Live stations:");
                for(const auto station : stations){
                    ImGui::Text("%s",station.name.c_str());
                }
            });
        }
    }
    std::string name;
    bool enabled = true;
    EventHandler<ImGui::WaterFall::FFTRedrawArgs> fftRedrawHandler;

    // Station data
    std::string downloadedData;
    std::unordered_map<int, std::vector<Station>> stations;
};

MOD_EXPORT void _INIT_()
{
    // Nothing
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
    // Nothing either
}