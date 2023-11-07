#ifndef SIMPLE_RTMP_FLV_H264_ENCODER_H
#define SIMPLE_RTMP_FLV_H264_ENCODER_H

#include <string>
#include <memory>
#include "channel.h"
#include "frame_buffer.h"
#include "flv_encoder.h"

namespace simple_rtmp
{
class flv_h264_encoder : public flv_encoder
{
   public:
    explicit flv_h264_encoder(std::string id);
    ~flv_h264_encoder() override = default;

   public:
    std::string id() override;
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec) override;
    void set_output(const channel::ptr& ch) override;

   private:
    void muxer_frame(const frame_buffer::ptr& frame, boost::system::error_code ec);

   private:
    struct args;
    std::string id_;
    channel::ptr ch_;
    std::shared_ptr<args> args_;
};

}    // namespace simple_rtmp

#endif
