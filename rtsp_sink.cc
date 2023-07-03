#include "rtsp_sink.h"

using simple_rtmp::rtsp_video_track;
using simple_rtmp::rtsp_audio_track;
using simple_rtmp::rtsp_track;

rtsp_track::ptr simple_rtmp::rtsp_video_track::clone() const
{
    return std::shared_ptr<rtsp_track>(new rtsp_video_track(*this));
}

std::string simple_rtmp::rtsp_video_track::id() const
{
    return kRtspVideoTrackId;
}
std::string simple_rtmp::rtsp_video_track::sdp() const
{
    return std::string();
}

rtsp_track::ptr simple_rtmp::rtsp_audio_track::clone() const
{
    return std::shared_ptr<rtsp_track>(new rtsp_audio_track(*this));
}
std::string simple_rtmp::rtsp_audio_track::sdp() const
{
    return std::string();
}
std::string simple_rtmp::rtsp_audio_track::id() const
{
    return kRtspAudioTrackId;
}
simple_rtmp::rtsp_sink::rtsp_sink(std::string id, simple_rtmp::executors::executor& ex) : id_(id), ex_(ex)
{
}
std::string simple_rtmp::rtsp_sink::id() const
{
    return std::string();
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
void simple_rtmp::rtsp_sink::add_codec(int codec)
{
}
void simple_rtmp::rtsp_sink::tracks(simple_rtmp::rtsp_sink::track_cb cb)
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
