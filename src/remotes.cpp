#include "remotes.h"
std::string getRemoteURL(bool local)
{
    return local ? "http://localhost:8000/" : "https://ottopattemore.github.io/shortwave-station-list/";
}