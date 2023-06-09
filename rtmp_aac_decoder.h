#ifndef SIMPLE_RTMP_AAC_DECODER_H
#define SIMPLE_RTMP_AAC_DECODER_H

#include <memory>
#include <string>
#include "frame_buffer.h"
#include "channel.h"
#include "rtmp_decoder.h"

namespace simple_rtmp
{
class rtmp_aac_decoder : public rtmp_decoder
{
   public:
    explicit rtmp_aac_decoder(std::string id);
    ~rtmp_aac_decoder() override;

   public:
    void write(const frame_buffer::ptr& frame) override;
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
