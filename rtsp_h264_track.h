#ifndef SIMPLE_RTMP_RTSP_H264_TRACK_H
#define SIMPLE_RTMP_RTSP_H264_TRACK_H

#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include "rtsp_track.h"

namespace simple_rtmp
{
class rtsp_h264_track : public rtsp_track
{
   public:
    rtsp_h264_track() = default;
    rtsp_h264_track(std::string sps, std::string pps);
    ~rtsp_h264_track() override = default;

   public:
    ptr clone() const override;
    std::string sdp() const override;
    std::string id() const override;

   private:
    std::stringstream ss_;
};

}    // namespace simple_rtmp
#endif
