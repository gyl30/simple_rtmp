#ifndef SIMPLE_RTMP_RTSP_HEVC_ENCODER_H
#define SIMPLE_RTMP_RTSP_HEVC_ENCODER_H

#include <string>
#include <memory>
#include <vector>
#include "rtsp_encoder.h"
#include "rtsp_hevc_track.h"

namespace simple_rtmp
{
class rtsp_hevc_encoder : public rtsp_encoder
{
   public:
    explicit rtsp_hevc_encoder(std::string id);
    ~rtsp_hevc_encoder() override = default;

   public:
    std::string id() override;
    rtsp_track::ptr track() override;
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec) override;
    void set_output(const channel::ptr& ch) override;

   private:
    void send_rtcp(const void* packet, int bytes, uint32_t timestamp, int flags);

   private:
    static int rtp_encode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int /*flags*/);

   private:
    std::string id_;
    channel::ptr ch_;
    frame_buffer::ptr vps_;
    frame_buffer::ptr sps_;
    frame_buffer::ptr pps_;
    rtsp_track::ptr track_;
    void* ctx_ = nullptr;
};
}    // namespace simple_rtmp

#endif
