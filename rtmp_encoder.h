#ifndef SIMPLE_RTMP_RTMP_ENCODER_H
#define SIMPLE_RTMP_RTMP_ENCODER_H

#include <string>
#include <functional>
#include "frame_buffer.h"
#include "channel.h"

namespace simple_rtmp
{
class rtmp_encoder
{
   public:
    rtmp_encoder()          = default;
    virtual ~rtmp_encoder() = default;

   public:
    virtual std::string id()                           = 0;
    virtual void write(const frame_buffer::ptr &frame) = 0;
    virtual void set_output(const channel::ptr &ch)    = 0;
};

}    // namespace simple_rtmp

#endif
