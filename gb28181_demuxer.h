#ifndef SIMPLE_RTMP_GB28181_DEMUXER_H
#define SIMPLE_RTMP_GB28181_DEMUXER_H

#include <memory>
#include <string>
#include <boost/system/error_code.hpp>
#include "frame_buffer.h"
#include "channel.h"
#include "rtmp_codec.h"

namespace simple_rtmp
{
class gb28181_demuxer : public std::enable_shared_from_this<gb28181_demuxer>
{
   public:
    using prt = std::shared_ptr<gb28181_demuxer>;

   public:
    explicit gb28181_demuxer(std::string& id);

   public:
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec);
    void on_codec(const std::function<void(int, codec_option)>& codec_cb);
    void set_channel(const channel::ptr& ch);

   private:
    std::string id_;
    channel::ptr ch_;
    std::function<void(int, codec_option)> codec_cb_;
};

}    // namespace simple_rtmp

#endif
