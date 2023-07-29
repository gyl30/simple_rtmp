#ifndef SIMPLE_RTMP_RTSP_H264_TRACK_H
#define SIMPLE_RTMP_RTSP_H264_TRACK_H

#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include "rtsp_track.h"
#include "frame_buffer.h"

namespace simple_rtmp
{
class rtsp_h264_track : public rtsp_track
{
   public:
    rtsp_h264_track() = default;
    rtsp_h264_track(const frame_buffer::ptr &sps, const frame_buffer::ptr &pps);
    ~rtsp_h264_track() override = default;

   public:
    std::string sdp() const override;
    uint32_t ssrc() const override;
    std::string id() const override;
    int32_t sample_rate() const override;

   private:
    int32_t sample_rate_ = 0;
    uint32_t ssrc_ = 0;
    std::stringstream ss_;
};

}    // namespace simple_rtmp
#endif
