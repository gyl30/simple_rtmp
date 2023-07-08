#include "rtsp_aac_encoder.h"

using simple_rtmp::rtsp_aac_encoder;

rtsp_aac_encoder::rtsp_aac_encoder(std::string id) : id_(std::move(id))
{
}
std::string simple_rtmp::rtsp_aac_encoder::id()
{
    return id_;
}
simple_rtmp::rtsp_track::ptr simple_rtmp::rtsp_aac_encoder::track()
{
    return track_;
}
void simple_rtmp::rtsp_aac_encoder::write(const simple_rtmp::frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
}
void simple_rtmp::rtsp_aac_encoder::set_output(const simple_rtmp::channel::ptr& ch)
{
}
