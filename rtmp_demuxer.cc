#include "rtmp_demuxer.h"

using simple_rtmp::rtmp_demuxer;

void rtmp_demuxer::set_channel(const channel::ptr& ch)
{
    ch_ = ch;
}

void rtmp_demuxer::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        return;
    }
}

void rtmp_demuxer::on_frame(const frame_buffer::ptr& frame)
{
    if (ch_)
    {
        ch_->write(frame, {});
    }
}
