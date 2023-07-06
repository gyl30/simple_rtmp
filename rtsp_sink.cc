#include <utility>
#include "rtsp_sink.h"

simple_rtmp::rtsp_sink::rtsp_sink(std::string id, simple_rtmp::executors::executor& ex) : id_(std::move(id)), ex_(ex)
{
}
std::string simple_rtmp::rtsp_sink::id() const
{
    return id_;
}
void simple_rtmp::rtsp_sink::write(const simple_rtmp::frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
}
void simple_rtmp::rtsp_sink::add_channel(const simple_rtmp::channel::ptr& ch)
{
}
void simple_rtmp::rtsp_sink::del_channel(const simple_rtmp::channel::ptr& ch)
{
}
void simple_rtmp::rtsp_sink::add_codec(int codec, codec_option op)
{
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
                tracks.push_back(video_track_->clone());
                tracks.push_back(audio_track_->clone());
                cb(tracks);
            }
        });
}
