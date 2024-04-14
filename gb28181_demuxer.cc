#include <cstring>
#include "gb28181_demuxer.h"
#include "log.h"

using simple_rtmp::gb28181_demuxer;

gb28181_demuxer::gb28181_demuxer(std::string& id) : id_(id)
{
}

void gb28181_demuxer::set_channel(const channel::ptr& ch)
{
    ch_ = ch;
}

void gb28181_demuxer::on_codec(const std::function<void(int, codec_option)>& codec_cb)
{
    codec_cb_ = codec_cb;
}

void gb28181_demuxer::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        return;
    }
}
