#ifndef SIMPLE_RTMP_RTSP_AAC_ENCODER_H
#define SIMPLE_RTMP_RTSP_AAC_ENCODER_H

#include <string>
#include <memory>
#include <vector>
#include <sstream>
#include "rtsp_encoder.h"
#include "rtsp_track.h"

namespace simple_rtmp
{
class rtsp_aac_encoder : public rtsp_encoder
{
   public:
    explicit rtsp_aac_encoder(std::string id);
    ~rtsp_aac_encoder() override = default;

   public:
    std::string id() override;
    rtsp_track::ptr track() override;
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec) override;
    void set_output(const channel::ptr& ch) override;

   private:
    static int rtp_encode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int /*flags*/);

   private:
    std::string id_;
    channel::ptr ch_;
    int sample_rate_ = 44100;
    int channel_count_ = 0;
    rtsp_track::ptr track_;
    void* ctx_ = nullptr;
};
}    // namespace simple_rtmp
#endif
