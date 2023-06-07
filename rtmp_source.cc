#include <utility>
#include "rtmp_source.h"
#include "rtmp_demuxer.h"
#include "log.h"

using simple_rtmp::rtmp_source;
using std::placeholders::_1;
using std::placeholders::_2;
using simple_rtmp::rtmp_demuxer;

rtmp_source::rtmp_source(std::string id) : id_(std::move(id)), ch_(std::make_shared<simple_rtmp::channel>()), demuxer_(std::make_shared<rtmp_demuxer>(id_))
{
    ch_->set_output(std::bind(&rtmp_source::on_frame, this, _1, _2));
    demuxer_->set_channel(ch_);
};

void rtmp_source::on_codec(int codec)
{
}

void rtmp_source::on_frame(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        return;
    }
    LOG_DEBUG("rtmp demuxer media {} codec {} pts {} dts {} paylaod {} bytes", frame->media, frame->codec, frame->pts, frame->dts, frame->payload.size());
}

void rtmp_source::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    demuxer_->write(frame, ec);
}

void simple_rtmp::rtmp_source::set_channel(const channel::ptr& ch)
{
    ch_ = ch;
}
