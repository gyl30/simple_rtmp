#ifndef SIMPLE_RTMP_AAC_ENCODER_H
#define SIMPLE_RTMP_AAC_ENCODER_H

#include "rtmp_encoder.h"
#include "frame_buffer.h"

namespace simple_rtmp
{
class rtmp_aac_encoder : public rtmp_encoder
{
   public:
    explicit rtmp_aac_encoder(std::string id);

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
