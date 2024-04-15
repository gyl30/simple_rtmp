#include "gb28181_rtp_demuxer.h"
using simple_rtmp::gb28181_rtp_demuxer;

simple_rtmp::frame_buffer::ptr gb28181_rtp_demuxer::write(const frame_buffer::ptr& frame)
{
    constexpr auto kGB28181RtpTcpPrefixSize = 2;

    data_.insert(data_.end(), frame->data(), frame->data() + frame->size());
    //
    return nullptr;
}
