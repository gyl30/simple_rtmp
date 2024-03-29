#include "rtsp_aac_track.h"

using simple_rtmp::rtsp_aac_track;

std::string simple_rtmp::rtsp_aac_track::sdp() const
{
    return ss_.str();
}

std::string simple_rtmp::rtsp_aac_track::id() const
{
    return kRtspAudioTrackId;
}

uint32_t simple_rtmp::rtsp_aac_track::ssrc() const
{
    return ssrc_;
}

int32_t rtsp_aac_track::sample_rate() const
{
    return sample_rate_;
}

rtsp_aac_track::rtsp_aac_track(const std::string& cfg, int sample_rate, int channels, int bitrate)
{
    ss_ << "m=audio 0 RTP/AVP 98\r\n";
    ss_ << "c=IN IP4 0.0.0.0\r\n";

    if (bitrate != 0)
    {
        ss_ << "b=AS:" << bitrate << "\r\n";
    }
    ss_ << "a=rtpmap:98 mpeg4-generic/" << sample_rate << "/" << channels << "\r\n";
    std::string config;
    char buf[8] = {0};
    for (const auto& ch : cfg)
    {
        snprintf(buf, sizeof(buf), "%02X", static_cast<uint8_t>(ch));
        config.append(buf);
    }
    ss_ << "a=fmtp:98 streamtype=5;profile-level-id=1;mode=AAC-hbr;"
        << "sizelength=13;indexlength=3;indexdeltalength=3;config=" << config << "\r\n";
    ss_ << "a=control:" << kRtspAudioTrackId << "\r\n";
    ssrc_ = rtsp_track::audio_ssrc();
    sample_rate_ = sample_rate;
}
