#include "utc.h"
#include <chrono>
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
