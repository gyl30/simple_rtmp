#ifndef SIMPLE_RTMP_FLV_ENCODER_H
#define SIMPLE_RTMP_FLV_ENCODER_H

#include <string>
#include "rtsp_track.h"
#include "frame_buffer.h"
#include "channel.h"

namespace simple_rtmp
{
class flv_encoder
{
   public:
    flv_encoder() = default;
    virtual ~flv_encoder() = default;

   public:
    virtual std::string id() = 0;
    virtual rtsp_track::ptr track() = 0;
    virtual void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec) = 0;
    virtual void set_output(const channel::ptr& ch) = 0;
};

}    // namespace simple_rtmp

#endif
