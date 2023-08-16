#include "rtsp_hevc_track.h"
#include "frame_buffer.h"

using simple_rtmp::rtsp_hevc_track;

using simple_rtmp::rtsp_track;

std::string simple_rtmp::rtsp_hevc_track::id() const
{
    return kRtspVideoTrackId;
}
std::string simple_rtmp::rtsp_hevc_track::sdp() const
{
    return ss_.str();
}
uint32_t simple_rtmp::rtsp_hevc_track::ssrc() const
{
    return ssrc_;
}
int32_t rtsp_hevc_track::sample_rate() const
{
    return 90000;
}

rtsp_hevc_track::rtsp_hevc_track(const frame_buffer::ptr &vps, const frame_buffer::ptr &sps, const frame_buffer::ptr &pps)
{
    ss_ << "m=video 0 RTP/AVP 96\r\n";
    ss_ << "c=IN IP4 0.0.0.0\n";
    ss_ << "a=rtpmap:96 H265/90000\r\n";
    ss_ << "a=fmtp:96 ";
    ss_ << "sprop-vps=" << base64_encode(vps->data(), vps->size()) << "; ";
    ss_ << "sprop-sps=" << base64_encode(sps->data(), sps->size()) << "; ";
    ss_ << "sprop-pps=" << base64_encode(pps->data(), pps->size()) << "\r\n";
    ss_ << "a=control:" << kRtspVideoTrackId << "\r\n";
    ssrc_ = rtsp_track::video_ssrc();
}
