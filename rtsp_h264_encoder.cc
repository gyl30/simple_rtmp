#include "rtsp_h264_encoder.h"

using simple_rtmp::rtsp_h264_encoder;

rtsp_h264_encoder::rtsp_h264_encoder(std::string id) : id_(std::move(id))
{
}

std::string simple_rtmp::rtsp_h264_encoder::id()
{
    return id_;
}

void rtsp_h264_encoder::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    // clang-format off
    enum { NAL_NIDR = 1, NAL_PARTITION_A = 2, NAL_IDR = 5, NAL_SEI = 6, NAL_SPS = 7, NAL_PPS = 8, NAL_AUD = 9, };
    // clang-format on

    if (frame->data()[4] == NAL_SPS)
    {
        sps_ = frame;
    }
    if (frame->data()[4] == NAL_PPS)
    {
        pps_ = frame;
    }
    // encode frame
}

void rtsp_h264_encoder::set_output(const simple_rtmp::channel::ptr& ch)
{
    ch_ = ch;
}

simple_rtmp::rtsp_track::ptr rtsp_h264_encoder::track()
{
    if (sps_ && pps_)
    {
        return std::make_shared<rtsp_h264_track>(sps_, pps_);
    }
    return nullptr;
}
