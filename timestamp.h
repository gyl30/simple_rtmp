#ifndef SIMPLE_RTMP_TIMESTAMP_H
#define SIMPLE_RTMP_TIMESTAMP_H

#include <cstdint>
#include <string>

namespace simple_rtmp
{
class timestamp
{
   private:
    explicit timestamp(int64_t milliseconds) : microseconds_(milliseconds){};

   public:
    int64_t milliseconds() const;
    int64_t microseconds() const;
    int64_t seconds() const;
    std::string fmt_milli_string() const;
    std::string fmt_micro_string() const;
    std::string fmt_second_string() const;

   public:
    static timestamp now();

   private:
    int64_t microseconds_;
};
}    // namespace simple_rtmp

#endif    //
