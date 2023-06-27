#ifndef SIMPLE_RTMP_RTMP_DECODER_H
#define SIMPLE_RTMP_RTMP_DECODER_H

#include <string>
#include <functional>
#include "frame_buffer.h"
#include "channel.h"

namespace simple_rtmp
{
class rtmp_decoder
{
   public:
    rtmp_decoder() = default;
    virtual ~rtmp_decoder() = default;

   public:
    virtual void write(const frame_buffer::ptr& frame, boost::system::error_code ec) = 0;
    virtual void set_output(const channel::ptr& ch) = 0;
};

}    // namespace simple_rtmp

#endif
