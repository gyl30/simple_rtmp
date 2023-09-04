#include "webrtc_sdp.h"

using simple_rtmp::webrtc_sdp;

webrtc_sdp::webrtc_sdp(const std::string& sdp) : sdp_(sdp)
{
}
int webrtc_sdp::parse()
{
    if (sdp_ == nullptr)
    {
        sdp_ = sdp_parse(sdp_str_.data(), static_cast<int>(sdp_str_.size()));
    }
    if (sdp_ == nullptr)
    {
        return -1;
    }
    int media_count = sdp_media_count(sdp_);
    if (sdp_origin_get_network(sdp_) == SDP_C_NETWORK_UNKNOWN)
    {
        return -1;
    }
    if (sdp_origin_get_addrtype(sdp_) == SDP_C_ADDRESS_UNKNOWN)
    {
        return -1;
    }
    if (sdp_session_get_name(sdp_) == nullptr)
    {
        return -1;
    }
    if (sdp_session_get_information(sdp_) == nullptr)
    {
        return -1;
    }
    if (sdp_version_get(sdp_) != 0)
    {
        return -1;
    }
    return 0;
}
