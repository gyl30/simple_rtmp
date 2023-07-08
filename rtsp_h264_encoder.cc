#include "rtsp_h264_encoder.h"
#include "rtsp_h264_track.h"

using simple_rtmp::rtsp_h264_encoder;

rtsp_h264_encoder::rtsp_h264_encoder(std::string id) : id_(std::move(id))
{
}

std::string simple_rtmp::rtsp_h264_encoder::id()
{
    return id_;
}

void simple_rtmp::rtsp_h264_encoder::write(const simple_rtmp::frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
}
void simple_rtmp::rtsp_h264_encoder::set_output(const simple_rtmp::channel::ptr& ch)
{
}
simple_rtmp::rtsp_track::ptr simple_rtmp::rtsp_h264_encoder::track()
{
    return track_;
}
