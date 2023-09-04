#ifndef SIMPLE_RTMP_WEBRTC_SDP_H
#define SIMPLE_RTMP_WEBRTC_SDP_H

#include <string>
#include "sdp.h"

namespace simple_rtmp
{
class webrtc_sdp
{
   public:
    explicit webrtc_sdp(const std::string& sdp);
    ~webrtc_sdp() = default;

   public:
    int parse();

   private:
    sdp_t* sdp_ = nullptr;
    std::string sdp_str_;
};
}    // namespace simple_rtmp

#endif
