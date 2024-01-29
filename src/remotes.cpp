#include "remotes.h"
#include <curl/curl.h>
#include <core.h>
#include <fstream>

RemoteSource::RemoteSource(const std::string &remote)
{
    std::string list;

    CURL *curl = curl_easy_init();
    if (!curl)
    {
        flog::error("[ Shortwave Station List ] Failed to init curl!");
        return;
    }
    // Download data
    curl_easy_setopt(curl, CURLOPT_URL, remote.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &list);
    size_t (*handler)(char *buffer, size_t itemSize, size_t nitems, void *ctx) = [](char *buffer, size_t itemSize, size_t nitems, void *ctx) -> size_t
    {
        size_t bytes = itemSize * nitems;
        std::string data;
        data.resize(bytes);
        memcpy(data.data(), buffer, bytes);
        ((std::string *)ctx)->append(data);
        return bytes;
    };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handler);
    CURLcode result = curl_easy_perform(curl);
    // Handle errors
    if (result != CURLE_OK)
    {
        flog::error("[ Shortwave Station List ] Failed to load {}", remote);
    }

    // Once the list is download decode the json
    nlohmann::json database = nullptr;
    try
    {
        database = json::parse(list);
    }
    catch (nlohmann::json::parse_error e)
    {
        flog::error("Error passing json from remote: {} \n\t{}",remote, e.what());
    }
    for (const auto station : database["stations"])
    {
        Station s;
        s.frequency = station["frequency"].get<int>();
        s.name = station["name"];
        s.power = station["power"].get<int>();
        s.notes = station["notes"];
        s.lat = station["location"][0].get<float>();
        s.lon = station["location"][1].get<float>();
        s.utcMin = std::stof(station["utc_start"].get<std::string>());
        s.utcMax = std::stof(station["utc_end"].get<std::string>());
        m_Stations[s.frequency].push_back(s);
    }
    flog::info("Fetch list: {}", remote);

    // Cleanup
    curl_easy_cleanup(curl);
}
std::unordered_map<int, std::vector<Station>> &RemoteSource::getStations()
{
    return m_Stations;
}
bool RemoteSource::isReadOnly()
{
    return true;
}

LocalSource::LocalSource(const std::filesystem::path& path)
{
    // Load local database
    std::ifstream inputFile(path);
    json database = nullptr;
    try
    {
        database = json::parse(inputFile);
    }
    catch (nlohmann::json::parse_error e)
    {
        flog::error("Error loading database: {}\n\t{}", path.c_str(), e.what());
        return;
    }

    for (const auto station : database["stations"])
    {
        Station s;
        s.frequency = station["frequency"].get<int>();
        s.name = station["name"];
        s.power = station["power"].get<int>();
        s.notes = station["notes"];
        s.lat = station["location"][0].get<float>();
        s.lon = station["location"][1].get<float>();
        s.utcMin = std::stof(station["utc_start"].get<std::string>());
        s.utcMax = std::stof(station["utc_end"].get<std::string>());
        m_Stations[s.frequency].push_back(s);
    }
    flog::info("[ Shortwave Station List ] Loaded from local source!");
}
StationList& LocalSource::getStations()
{
    return m_Stations;
}
bool LocalSource::isReadOnly()
{
    return false;
}
void LocalSource::save()
{
}
