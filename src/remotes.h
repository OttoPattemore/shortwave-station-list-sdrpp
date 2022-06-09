#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "station.h"
#include <filesystem>
using StationList = std::unordered_map<int, std::vector<Station>>;

class DataSource
{
    public:
        virtual StationList& getStations() = 0;
        virtual void save(){};
        virtual bool isReadOnly() = 0;
};

class RemoteSource : public DataSource
{
    public:
        RemoteSource(const std::string& remote);
        StationList& getStations() override;
        bool isReadOnly() override;
    private:
        StationList m_Stations;
};
class LocalSource : public DataSource
{
    public:
        LocalSource(const std::filesystem::path& path);
        StationList& getStations() override;
        bool isReadOnly() override;
        void save() override;
    private:
        StationList m_Stations;
};