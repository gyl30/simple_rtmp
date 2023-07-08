#ifndef SIMPLE_RTMP_RTSP_H264_ENCODER_H
#define SIMPLE_RTMP_RTSP_H264_ENCODER_H

#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include "rtsp_encoder.h"
#include "rtsp_track.h"

namespace simple_rtmp
{
class rtsp_h264_encoder : public rtsp_encoder
{
   public:
    explicit rtsp_h264_encoder(std::string id);
    ~rtsp_h264_encoder() override = default;

   public:
    std::string id() override;
    rtsp_track::ptr track() override;
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec) override;
    void set_output(const channel::ptr& ch) override;

   private:
    std::string id_;
    rtsp_track::ptr track_;
};
}    // namespace simple_rtmp
#endif
