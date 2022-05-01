#include <imgui.h>
#include <config.h>
#include <core.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <module.h>
#include <gui/gui.h>
#include <dsp/pll.h>
#include <dsp/stream.h>
#include <dsp/demodulator.h>
#include <dsp/window.h>
#include <dsp/resampling.h>
#include <dsp/processing.h>
#include <dsp/routing.h>

#include <dsp/deframing.h>
#include <dsp/sink.h>

#include <gui/widgets/symbol_diagram.h>

#include <fstream>

SDRPP_MOD_INFO{
    /* Name:            */ "Shortwave Staton List",
    /* Description:     */ "Plugin to show data from shortwave-station-list in SDR++",
    /* Author:          */ "Otto Pattemore",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1};


class ShortwaveStationList : public ModuleManager::Instance
{
public:
    ShortwaveStationList(std::string name)
    {
        gui::menu.registerEntry(name, menuHandler, this, NULL);
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
    static void menuHandler(void *ctx)
    {
        ShortwaveStationList * _this = (ShortwaveStationList *)ctx;
        ImGui::Text("Hello!");
    }

    std::string name;
    bool enabled = true;
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