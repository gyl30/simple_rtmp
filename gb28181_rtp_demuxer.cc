#include "gb28181_rtp_demuxer.h"
using simple_rtmp::gb28181_rtp_demuxer;

void gb28181_rtp_demuxer::set_channel(const channel::ptr& ch)
{
    ch_ = ch;
}

void gb28181_rtp_demuxer::write(const frame_buffer::ptr& frame)
{
    constexpr auto kGB28181RtpTcpPrefixSize = 2;

    data_.insert(data_.end(), frame->data(), frame->data() + frame->size());

    if (data_.size() < kGB28181RtpTcpPrefixSize)
    {
        return;
    }
}
