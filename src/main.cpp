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
ConfigManager config;
float getUTCTime()
{
    std::time_t now = std::time(0);
    std::tm *now_tm = std::gmtime(&now);
    return now_tm->tm_min + (now_tm->tm_hour * 100);
}
int getUTCHour()
{
    std::time_t now = std::time(0);
    std::tm *now_tm = std::gmtime(&now);
    return now_tm->tm_hour;
}
int getUTCMin()
{
    std::time_t now = std::time(0);
    std::tm *now_tm = std::gmtime(&now);
    return now_tm->tm_min;
}
class ShortwaveStationList : public ModuleManager::Instance
{
public:
    ShortwaveStationList(std::string name)
    {
        gui::menu.registerEntry(name, menuHandler, this, NULL);

        fftRedrawHandler.ctx = this;
        fftRedrawHandler.handler = fftRedraw;
        gui::waterfall.onFFTRedraw.bindHandler(&fftRedrawHandler);

        // Load config
        config.acquire();
        config.conf["test"] = "HIHI";
        config.release(true);

        loadDatabase();
    }
    void loadDatabase()
    {
        stations.clear();
        std::string url = useLocalHost ? "http://localhost:8000/" : "http://ottopattemore.github.io/shortwave-station-list/";
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
            std::cout<<itemSize*nitems<<std::endl;
            size_t bytes = itemSize * nitems;
            std::string data;
            data.resize(bytes);
            memcpy(data.data(), buffer,bytes);
            ((std::string*)ctx)->append(data);
            return bytes; };
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
        // Cleanup
        curl_easy_cleanup(curl);
    }
    static void menuHandler(void *ctx)
    {
        ShortwaveStationList * _this = (ShortwaveStationList *)ctx;
        ImGui::TextColored(ImVec4(0.5,0.9,0.5,1),"Current UTC Time: %02i:%02i", getUTCHour(),getUTCMin());
        ImGui::Checkbox("Display on FFT", &_this->showStations);
        if (ImGui::BeginCombo("Source", _this->useLocalHost ? "Local Host ( For testing )" : "Remote ( Default )"))
        {
            if (ImGui::Selectable("Remote ( Default )"))
            {
                _this->useLocalHost = false;
                _this->loadDatabase();
            }
            if (ImGui::Selectable("Local Host ( For testing )"))
            {
                _this->useLocalHost = true;
                _this->loadDatabase();
            }
            ImGui::EndCombo();
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
    }
    static void fftRedraw(ImGui::WaterFall::FFTRedrawArgs args, void *ctx)
    {
        ShortwaveStationList *_this = (ShortwaveStationList *)ctx;
        if (!_this->showStations) return;
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
                if ( (getUTCTime() > station.utcMin) && (getUTCTime() < station.utcMax) ){
                    liveStations.push_back(station);
                }
            }

            // There are no live stations
            if(!liveStations.size()) continue;

            const std::string text = liveStations[0].name;
            draw(text, frequency * 1000, [&liveStations,frequency]()
                {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.5,0.5,1,1), "Live stations on %ikHz:", frequency);
                    for (const auto station : liveStations)
                    {
                        ImGui::Text("%s\t[%i - %i]\t%ikm", station.name.c_str(), station.utcMin, station.utcMax, 0);
                    }
                    ImGui::Separator();
                }
            );
        }
    }
    bool useLocalHost = false;
    bool showStations = true;
    std::string name;
    bool enabled = true;
    EventHandler<ImGui::WaterFall::FFTRedrawArgs> fftRedrawHandler;
    std::unordered_map<int, std::vector<Station>> stations;
    bool errorLoading = false;
};

MOD_EXPORT void _INIT_()
{
    json def = json({});
    config.setPath(core::args["root"].s() + "/shortwave_station_list.json");
    config.load(def);
    config.enableAutoSave();
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