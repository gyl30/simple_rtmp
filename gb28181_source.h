#ifndef SIMPLE_RTMP_GB28181_SOURCE_H
#define SIMPLE_RTMP_GB28181_SOURCE_H

#include <string>
#include <utility>
#include <memory>
#include <boost/system/error_code.hpp>
#include "frame_buffer.h"
#include "execution.h"
#include "channel.h"
#include "rtmp_codec.h"

namespace simple_rtmp
{

class gb28181_source : public std::enable_shared_from_this<gb28181_source>
{
   public:
    using prt = std::shared_ptr<gb28181_source>;

    explicit gb28181_source(std::string id, simple_rtmp::executors::executor& ex);

   public:
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec);
    void on_codec(int codec, codec_option op);
    void set_channel(const channel::ptr& ch);

   private:
    void on_frame(const frame_buffer::ptr& frame, const boost::system::error_code& ec);

   private:
    std::string id_;
    channel::ptr ch_;
};

}    // namespace simple_rtmp

#endif
