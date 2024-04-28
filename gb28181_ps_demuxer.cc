#include "gb28181_ps_demuxer.h"

using simple_rtmp::gb28181_ps_demuxer;

gb28181_ps_demuxer::gb28181_ps_demuxer(std::string id) : id_(std::move(id))
{
}
gb28181_ps_demuxer::~gb28181_ps_demuxer() = default;

void gb28181_ps_demuxer::set_channel(const channel::ptr& ch)
{
    ch_ = ch;
}

void gb28181_ps_demuxer::on_frame(const frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ch_)
    {
        ch_->write(frame, ec);
    }
}
void gb28181_ps_demuxer::write(const frame_buffer::ptr& frame)
{
}
