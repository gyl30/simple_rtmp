#include "flv_sink.h"
#include "flv-writer.h"
#include "flv-header.h"
#include "flv_h264_encoder.h"
#include "log.h"

using simple_rtmp::flv_sink;

static simple_rtmp::frame_buffer::ptr make_flv_header()
{
    struct flv_vec_t vec[1];
    uint8_t header[9 + 4];
    flv_header_write(1, 1, header, 9);
    flv_tag_size_write(header + 9, 4, 0);
    return simple_rtmp::fixed_frame_buffer::create(header, sizeof(header));
}

flv_sink::flv_sink(std::string id, simple_rtmp::executors::executor& ex) : id_(std::move(id)), ex_(ex)
{
}
std::string flv_sink::id() const
{
    return id_;
}
void flv_sink::write(const simple_rtmp::frame_buffer::ptr& frame, const boost::system::error_code& ec)
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
void flv_sink::del_channel(const channel::ptr& ch)
{
    ex_.post(std::bind(&flv_sink::safe_del_channel, this, ch));
}

void flv_sink::safe_del_channel(const channel::ptr& ch)
{
    chs_.erase(ch);
}

void flv_sink::add_channel(const channel::ptr& ch)
{
    ex_.post(std::bind(&flv_sink::safe_add_channel, this, ch));
}

void flv_sink::safe_add_channel(const channel::ptr& ch)
{
    auto frame = make_flv_header();
    ch->write(frame, {});
    chs_.insert(ch);
}
void flv_sink::on_frame(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    for (const auto& ch : chs_)
    {
        ch->write(frame, ec);
    }
}
void flv_sink::add_codec(int codec, codec_option op)
{
    (void)op;
    if (codec == simple_rtmp::rtmp_codec::h264)
    {
        video_encoder_ = std::make_shared<flv_h264_encoder>(id_);
        auto ch = std::make_shared<simple_rtmp::channel>();
        ch->set_output(std::bind(&flv_sink::on_frame, this, std::placeholders::_1, std::placeholders::_2));
        video_encoder_->set_output(ch);
        LOG_DEBUG("{} add flv h264 encoder", id_);
    }
    else if (codec == simple_rtmp::rtmp_codec::aac)
    {
    }
}
