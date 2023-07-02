#ifndef SIMPLE_RTMP_RTMP_H264_ENCODER_H
#define SIMPLE_RTMP_RTMP_H264_ENCODER_H

#include <string>
#include "rtmp_encoder.h"

namespace simple_rtmp
{

class rtmp_h264_encoder : public rtmp_encoder
{
   public:
    explicit rtmp_h264_encoder(std::string id);
    ~rtmp_h264_encoder() override = default;

   public:
    std::string id() override;
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec) override;
    void set_output(const channel::ptr& ch) override;

   private:
    void on_frame(const frame_buffer::ptr& frame, boost::system::error_code ec);

   private:
    std::string id_;
    channel::ptr ch_;
    struct args;
    std::shared_ptr<args> args_;
};

}    // namespace simple_rtmp

#endif
