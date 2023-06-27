#include <ctime>         // gmtime_r
#include <sys/time.h>    // gettimeofday
#include "timestamp.h"

using simple_rtmp::timestamp;

static const int kMicroSecondsPerSecond = 1000 * 1000;
static const int kMicroSecondsPerMilli = 1000;

static std::string format_time(int64_t microseconds, bool mill, bool micro)
{
    char buf[64] = {0};
    auto seconds = static_cast<time_t>(microseconds / kMicroSecondsPerSecond);
    struct tm tm_time;
    gmtime_r(&seconds, &tm_time);
    if (micro)
    {
        int const micro_seconds = static_cast<int>(microseconds % kMicroSecondsPerSecond);
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d", tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, micro_seconds);
    }
    else if (mill)
    {
        int const microseconds1 = static_cast<int>(microseconds % kMicroSecondsPerSecond);
        int const milliseconds = microseconds1 / kMicroSecondsPerMilli;
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%03d", tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, milliseconds);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d", tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    }
    return buf;
}

int64_t timestamp::microseconds() const
{
    return microseconds_;
}

int64_t timestamp::milliseconds() const
{
    return microseconds_ / kMicroSecondsPerMilli;
}

int64_t timestamp::seconds() const
{
    return microseconds_ / kMicroSecondsPerSecond;
}

timestamp timestamp::now()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    int64_t const seconds = tv.tv_sec;
    return timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}
std::string timestamp::fmt_micro_string() const
{
    constexpr bool mill = false;
    constexpr bool micro = true;
    return format_time(microseconds_, mill, micro);
}
std::string timestamp::fmt_milli_string() const
{
    constexpr bool mill = true;
    constexpr bool micro = false;
    return format_time(microseconds_, mill, micro);
}
std::string timestamp::fmt_second_string() const
{
    constexpr bool mill = false;
    constexpr bool micro = false;

    return format_time(microseconds_, mill, micro);
}
