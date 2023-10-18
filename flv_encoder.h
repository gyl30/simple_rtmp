#ifndef SIMPLE_RTMP_FLV_ENCODER_H
#define SIMPLE_RTMP_FLV_ENCODER_H

#include <set>
#include <string>
#include <memory>
#include <functional>
#include "frame_buffer.h"
#include "channel.h"

namespace simple_rtmp
{
class flv_encoder
{
   public:
    explicit flv_encoder(std::string id);
    ~flv_encoder() = default;

   public:
    std::string id();
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec);
    void set_output(const channel::ptr& ch);

   private:
    struct args;
    std::string id_;
    std::set<channel::ptr> channels_;
    std::shared_ptr<args> args_;
};

}    // namespace simple_rtmp

#endif
