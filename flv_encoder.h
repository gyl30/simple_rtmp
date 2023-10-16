#ifndef SIMPLE_RTMP_FLV_ENCODER_H
#define SIMPLE_RTMP_FLV_ENCODER_H

#include <string>
#include <functional>
#include "frame_buffer.h"
#include "channel.h"

namespace simple_rtmp
{
class flv_encoder
{
   public:
    flv_encoder() = default;
    ~flv_encoder() = default;

   public:
    std::string id() 0;
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec);
    void set_output(const channel::ptr& ch);
};

}    // namespace simple_rtmp

#endif
