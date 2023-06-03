#ifndef SIMPLE_RTMP_RTMP_DEMUXER_H
#define SIMPLE_RTMP_RTMP_DEMUXER_H

#include <memory>
#include <boost/system/error_code.hpp>
#include "frame_buffer.h"
#include "channel.h"

namespace simple_rtmp
{
class rtmp_demuxer : public std::enable_shared_from_this<rtmp_demuxer>
{
   public:
    using prt = std::shared_ptr<rtmp_demuxer>;

   public:
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec);
    void set_channel(const channel::ptr& ch);

   private:
    void on_frame(const frame_buffer::ptr& frame);

   private:
    channel::ptr ch_;
};

}    // namespace simple_rtmp

#endif
