#ifndef SIMPLE_RTMP_WEBRTC_SDP_H
#define SIMPLE_RTMP_WEBRTC_SDP_H

#include <string>
#include "sdp.h"

class webrtc_sdp
{
   public:
    explicit webrtc_sdp(const std::string& sdp);
    ~webrtc_sdp() = default;

   public:
    void parse();

   private:
    std::string sdp_;
};

#endif
