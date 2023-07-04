#include "rtsp_sink.h"
#include <boost/beast/core/detail/base64.hpp>
#include <utility>

using simple_rtmp::rtsp_video_track;
using simple_rtmp::rtsp_audio_track;
using simple_rtmp::rtsp_track;

std::string base64_encode(const std::uint8_t* data, std::size_t len)
{
    std::string dest;
    dest.resize(boost::beast::detail::base64::encoded_size(len));
    dest.resize(boost::beast::detail::base64::encode(&dest[0], data, len));
    return dest;
}

std::string base64_encode(const std::string& s)
{
    return base64_encode(reinterpret_cast<std::uint8_t const*>(s.data()), s.size());
}

std::string base64_decode(const std::string& data)
{
    std::string dest;
    dest.resize(boost::beast::detail::base64::decoded_size(data.size()));
    auto const result = boost::beast::detail::base64::decode(&dest[0], data.data(), data.size());
    dest.resize(result.first);
    return dest;
}

rtsp_video_track::rtsp_video_track(std::string sps, std::string pps)
{
    ss_ << "m=video 0 RTP/AVP 96\r\n";
    ss_ << "a=rtpmap:96 H264/90000\r\n";
    ss_ << "a=fmtp:96 packetization-mode=1; profile-level-id=";
    uint32_t profile_level_id = 0;
    if (sps.size() >= 4)
    {
        profile_level_id = (uint8_t(sps[1]) << 16) | (uint8_t(sps[2]) << 8) | (uint8_t(sps[3]));
    }

    char profile[32] = {0};
    snprintf(profile, sizeof(profile), "%06X", profile_level_id);
    ss_ << profile << "; sprop-parameter-sets=";
    ss_ << base64_encode(sps) << "," << base64_encode(pps) << "\r\n";
    ss_ << "a=control:" << kRtspVideoTrackId << "\r\n";
}
rtsp_track::ptr simple_rtmp::rtsp_video_track::clone() const
{
    auto* self = new rtsp_video_track();
    self->ss_ << this->ss_.rdbuf();
    return std::shared_ptr<rtsp_track>(self);
}

std::string simple_rtmp::rtsp_video_track::id() const
{
    return kRtspVideoTrackId;
}
std::string simple_rtmp::rtsp_video_track::sdp() const
{
    return ss_.str();
}

rtsp_audio_track::rtsp_audio_track(const std::string& cfg, int sample_rate, int channels, int bitrate)
{
    ss_ << "m=audio 0 RTP/AVP 98\r\n";
    if (bitrate)
    {
        ss_ << "b=AS:" << bitrate << "\r\n";
    }
    ss_ << "a=rtpmap:98 mpeg4-generic/" << sample_rate << "/" << channels << "\r\n";
    std::string config;
    char buf[8] = {0};
    for (const auto& ch : cfg)
    {
        snprintf(buf, sizeof(buf), "%02X", (uint8_t)ch);
        config.append(buf);
    }
    ss_ << "a=fmtp:98 streamtype=5;profile-level-id=1;mode=AAC-hbr;"
        << "sizelength=13;indexlength=3;indexdeltalength=3;config=" << config << "\r\n";
    ss_ << "a=control:" << kRtspAudioTrackId << "\r\n";
}
rtsp_track::ptr simple_rtmp::rtsp_audio_track::clone() const
{
    auto* self = new rtsp_audio_track();
    self->ss_ << this->ss_.rdbuf();
    return std::shared_ptr<rtsp_track>(self);
}
std::string simple_rtmp::rtsp_audio_track::sdp() const
{
    return ss_.str();
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
