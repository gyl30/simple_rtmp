#include "rtmp_sink.h"

using simple_rtmp::rtmp_sink;

std::string rtmp_sink::id() const
{
    return id_;
}

void rtmp_sink::add_channel(const channel::ptr& ch)
{
    chs_.insert(ch);
}

void rtmp_sink::del_channel(const channel::ptr& ch)
{
    chs_.erase(ch);
}

void rtmp_sink::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        return;
    }
    for (auto& ch : chs_)
    {
        ch->write(frame, ec);
    }
}
