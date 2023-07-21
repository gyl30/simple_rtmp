#include <utility>
#include "rtsp_sink.h"
#include "rtmp_codec.h"
#include "rtsp_h264_encoder.h"
#include "rtsp_aac_encoder.h"
#include "log.h"

using simple_rtmp::rtsp_sink;
using std::placeholders::_1;
using std::placeholders::_2;

simple_rtmp::rtsp_sink::rtsp_sink(std::string id, simple_rtmp::executors::executor& ex) : id_(std::move(id)), ex_(ex)
{
}
std::string simple_rtmp::rtsp_sink::id() const
{
    return id_;
}
void simple_rtmp::rtsp_sink::write(const simple_rtmp::frame_buffer::ptr& frame, const boost::system::error_code& ec)
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
void simple_rtmp::rtsp_sink::add_channel(const simple_rtmp::channel::ptr& ch)
{
    chs_.insert(ch);
}
void simple_rtmp::rtsp_sink::del_channel(const simple_rtmp::channel::ptr& ch)
{
    chs_.erase(ch);
}

void simple_rtmp::rtsp_sink::add_codec(int codec, codec_option op)
{
    if (codec == simple_rtmp::rtmp_codec::h264)
    {
        video_encoder_ = std::make_shared<rtsp_h264_encoder>(id_);
        auto ch = std::make_shared<simple_rtmp::channel>();
        ch->set_output(std::bind(&rtsp_sink::on_frame, this, _1, _2));
        video_encoder_->set_output(ch);
        LOG_DEBUG("{} add rtsp h264 encoder", id_);
    }
    else if (codec == simple_rtmp::rtmp_codec::aac)
    {
        audio_encoder_ = std::make_shared<rtsp_aac_encoder>(id_);
        auto ch = std::make_shared<simple_rtmp::channel>();
        ch->set_output(std::bind(&rtsp_sink::on_frame, this, _1, _2));
        audio_encoder_->set_output(ch);
        LOG_DEBUG("{} add rtsp aac encoder", id_);
    }
}
void rtsp_sink::on_frame(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    for (const auto& ch : chs_)
    {
        ch->write(frame, ec);
    }
}
void simple_rtmp::rtsp_sink::tracks(const simple_rtmp::rtsp_sink::track_cb& cb)
{
    auto self = shared_from_this();
    ex_.post(
        [this, self, cb]()
        {
            if (cb)
            {
                std::vector<rtsp_track::ptr> tracks;
                if (video_encoder_)
                {
                    tracks.push_back(video_encoder_->track());
                }
                if (audio_encoder_)
                {
                    tracks.push_back(audio_encoder_->track());
                }

                cb(tracks);
            }
        });
}
