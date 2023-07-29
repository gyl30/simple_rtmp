#include "rtsp_h264_track.h"
#include "frame_buffer.h"

using simple_rtmp::rtsp_h264_track;
using simple_rtmp::rtsp_track;

std::string simple_rtmp::rtsp_h264_track::id() const
{
    return kRtspVideoTrackId;
}
std::string simple_rtmp::rtsp_h264_track::sdp() const
{
    return ss_.str();
}
uint32_t simple_rtmp::rtsp_h264_track::ssrc() const
{
    return ssrc_;
}
int32_t rtsp_h264_track::sample_rate() const
{
    return 90000;
}

rtsp_h264_track::rtsp_h264_track(const frame_buffer::ptr &sps, const frame_buffer::ptr &pps)
{
    ss_ << "m=video 0 RTP/AVP 96\r\n";
    ss_ << "c=IN IP4 0.0.0.0\n";
    ss_ << "a=rtpmap:96 H264/90000\r\n";
    ss_ << "a=fmtp:96 packetization-mode=1; profile-level-id=";
    uint32_t profile_level_id = 0;
    const uint8_t *sps_data = sps->data();
    if (sps->size() >= 4)
    {
        profile_level_id = ((sps_data[1]) << 16) | ((sps_data[2]) << 8) | ((sps_data[3]));
    }

    char profile[32] = {0};
    snprintf(profile, sizeof(profile), "%06X", profile_level_id);
    ss_ << profile << "; sprop-parameter-sets=";
    ss_ << base64_encode(sps->data(), sps->size()) << "," << base64_encode(pps->data(), pps->size()) << "\r\n";
    ss_ << "a=control:" << kRtspVideoTrackId << "\r\n";
    ssrc_ = rtsp_track::video_ssrc();
}
