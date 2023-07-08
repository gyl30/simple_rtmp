#ifndef SIMPLE_RTMP_RTSP_AAC_TRACK_H
#define SIMPLE_RTMP_RTSP_AAC_TRACK_H

#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include "rtsp_track.h"

namespace simple_rtmp
{
class rtsp_aac_track : public rtsp_track
{
   public:
    rtsp_aac_track() = default;
    rtsp_aac_track(const std::string& cfg, int sample_rate, int channels, int bitrate);
    ~rtsp_aac_track() override = default;

   public:
    std::string sdp() const override;
    std::string id() const override;

   private:
    std::stringstream ss_;
};
}    // namespace simple_rtmp

#endif
