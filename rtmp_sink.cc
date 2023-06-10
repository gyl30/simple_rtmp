#include "rtmp_sink.h"
#include "rtmp_codec.h"
#include "log.h"

using simple_rtmp::rtmp_sink;

rtmp_sink ::rtmp_sink(std::string id, simple_rtmp::executors::executor& ex) : id_(std::move(id)), ex_(ex)
{
}

std::string rtmp_sink::id() const
{
    return id_;
}

void rtmp_sink::del_channel(const channel::ptr& ch)
{
    ex_.post(std::bind(&rtmp_sink::safe_del_channel, this, ch));
}

void rtmp_sink::safe_del_channel(const channel::ptr& ch)
{
    chs_.erase(ch);
}

void rtmp_sink::add_channel(const channel::ptr& ch)
{
    ex_.post(std::bind(&rtmp_sink::safe_add_channel, this, ch));
}

void rtmp_sink::safe_add_channel(const channel::ptr& ch)
{
    if (video_config_)
    {
        LOG_DEBUG("write video config frame {} bytes", video_config_->payload.size());
        ch->write(video_config_, {});
    }
    if (audio_config_)
    {
        LOG_DEBUG("write audio config frame {} bytes", audio_config_->payload.size());
        ch->write(audio_config_, {});
    }
    for (const auto& frame : gop_cache_)
    {
        ch->write(frame, {});
    }
    chs_.insert(ch);
}

static bool audio_config_frame(const simple_rtmp::frame_buffer::ptr& frame)
{
    return (frame->codec == simple_rtmp::rtmp_codec::aac || frame->codec == simple_rtmp::rtmp_codec::opus) && frame->payload[1] == 0;
}

static bool video_config_frame(const simple_rtmp::frame_buffer::ptr& frame)
{
    return frame->payload[1] == 0;
}

void rtmp_sink::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        return;
    }
    if (frame->media == simple_rtmp::rtmp_tag::video && (video_config_frame(frame)))
    {
        video_config_ = frame;
    }
    else if (frame->media == simple_rtmp::rtmp_tag::audio && (audio_config_frame(frame)))
    {
        audio_config_ = frame;
    }

    if (frame->flag == 1)
    {
        gop_cache_.clear();
        gop_cache_.push_back(frame);
    }
    if (!gop_cache_.empty() && frame->flag != 1)
    {
        gop_cache_.push_back(frame);
    }

    for (auto& ch : chs_)
    {
        ch->write(frame, ec);
    }
}
