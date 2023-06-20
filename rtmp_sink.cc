#include "rtmp_sink.h"
#include "rtmp_codec.h"
#include "rtmp_h264_encoder.h"
#include "rtmp_aac_encoder.h"
#include "log.h"

using simple_rtmp::rtmp_sink;

rtmp_sink ::rtmp_sink(std::string id, simple_rtmp::executors::executor& ex) : id_(std::move(id)), ex_(ex)
{
}

std::string rtmp_sink::id() const
{
    return id_;
}

void rtmp_sink::add_codec(int codec)
{
    LOG_DEBUG("{} add codec {}", id_, codec);
    if (codec == simple_rtmp::rtmp_codec::h264)
    {
        video_encoder_ = std::make_shared<rtmp_h264_encoder>(id_);
        auto ch        = std::make_shared<simple_rtmp::channel>();
        ch->set_output(std::bind(&rtmp_sink::on_frame, this, std::placeholders::_1, std::placeholders::_2));
        video_encoder_->set_output(ch);
        LOG_DEBUG("{} add h264 encoder", id_);
    }
    else if (codec == simple_rtmp::rtmp_codec::aac)
    {
        audio_encoder_ = std::make_shared<rtmp_aac_encoder>(id_);
        auto ch        = std::make_shared<simple_rtmp::channel>();
        ch->set_output(std::bind(&rtmp_sink::on_frame, this, std::placeholders::_1, std::placeholders::_2));
        audio_encoder_->set_output(ch);
        LOG_DEBUG("{} add aac encoder", id_);
    }
}

static bool audio_config_frame(const simple_rtmp::frame_buffer::ptr& frame)
{
    return (frame->codec() == simple_rtmp::rtmp_codec::aac || frame->codec() == simple_rtmp::rtmp_codec::opus) && frame->data()[1] == 0;
}

static bool video_config_frame(const simple_rtmp::frame_buffer::ptr& frame)
{
    return frame->data()[1] == 0;
}

void rtmp_sink::on_frame(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        for (const auto& ch : chs_)
        {
            ch->write(frame, ec);
        }
        return;
    }
    if (frame->media() == simple_rtmp::rtmp_tag::video)
    {
        on_video_frame(frame);
    }
    else if (frame->media() == simple_rtmp::rtmp_tag::audio)
    {
        on_audio_frame(frame);
    }

    for (const auto& ch : chs_)
    {
        ch->write(frame, ec);
    }
}
void rtmp_sink::on_video_frame(const frame_buffer::ptr& frame)
{
    if (video_config_frame(frame))
    {
        video_config_ = frame;
        return;
    }

    if (frame->flag() == 1)
    {
        gop_cache_.clear();
        gop_cache_.push_back(frame);
        return;
    }
    if (!gop_cache_.empty())
    {
        gop_cache_.push_back(frame);
    }
}
void rtmp_sink::on_audio_frame(const frame_buffer::ptr& frame)
{
    if (audio_config_frame(frame))
    {
        audio_config_ = frame;
    }
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
        LOG_DEBUG("write video config frame {} bytes", video_config_->size());
        ch->write(video_config_, {});
    }
    if (audio_config_)
    {
        LOG_DEBUG("write audio config frame {} bytes", audio_config_->size());
        ch->write(audio_config_, {});
    }
    for (const auto& frame : gop_cache_)
    {
        ch->write(frame, {});
    }
    chs_.insert(ch);
}

void rtmp_sink::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        if (video_encoder_)
        {
            video_encoder_->write(frame, ec);
        }
        if (audio_encoder_)
        {
            audio_encoder_->write(frame, ec);
        }
        return;
    }

    if (frame->media() == simple_rtmp::rtmp_tag::video && video_encoder_)
    {
        video_encoder_->write(frame, ec);
    }
    if (frame->media() == simple_rtmp::rtmp_tag::audio && audio_encoder_)
    {
        audio_encoder_->write(frame, ec);
    }
}
