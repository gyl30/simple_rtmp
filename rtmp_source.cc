#include <utility>
#include "rtmp_source.h"
#include "rtmp_demuxer.h"
#include "rtmp_sink.h"
#include "log.h"

using simple_rtmp::rtmp_source;
using namespace std::placeholders;
using simple_rtmp::rtmp_demuxer;

rtmp_source::rtmp_source(std::string id, simple_rtmp::executors::executor& ex) : id_(std::move(id)), ch_(std::make_shared<simple_rtmp::channel>()), demuxer_(std::make_shared<rtmp_demuxer>(id_))
{
    ch_->set_output(std::bind(&rtmp_source::on_frame, this, _1, _2));
    demuxer_->set_channel(ch_);
    demuxer_->on_codec(std::bind(&rtmp_source::on_codec, this, _1, _2));
    std::string const rtmp_sink_id = "rtmp_" + id_;
    rtmp_sink_ = std::make_shared<simple_rtmp::rtmp_sink>(rtmp_sink_id, ex);
    simple_rtmp::sink::add(rtmp_sink_);
};

void rtmp_source::on_codec(int codec, codec_option op)
{
    rtmp_sink_->add_codec(codec, std::move(op));
}

void rtmp_source::on_frame(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    // LOG_DEBUG("rtmp demuxer media {} codec {} pts {} dts {} paylaod {} bytes", simple_rtmp::rtmp_tag_to_str(frame->media), simple_rtmp::rtmp_codec_to_str(frame->codec), frame->pts, frame->dts,
    // frame->payload.size());
    rtmp_sink_->write(frame, ec);
}

void rtmp_source::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    demuxer_->write(frame, ec);
}

void simple_rtmp::rtmp_source::set_channel(const channel::ptr& ch)
{
    ch_ = ch;
}
