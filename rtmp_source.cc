#include "rtmp_source.h"

#include <utility>
#include "rtmp_demuxer.h"

using simple_rtmp::rtmp_source;
using simple_rtmp::rtmp_demuxer;

rtmp_source::rtmp_source(std::string id) : id_(std::move(id)), demuxer_(std::make_shared<rtmp_demuxer>(id_)){};

void rtmp_source::on_frame(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        return;
    }
    if (ch_)
    {
        ch_->write(frame, ec);
    }
}

void rtmp_source::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    demuxer_->write(frame, ec);
}

void simple_rtmp::rtmp_source::set_channel(const channel::ptr& ch)
{
    ch_ = ch;
}
