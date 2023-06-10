#ifndef SIMPLE_RTMP_RTMP_SOURCE_H
#define SIMPLE_RTMP_RTMP_SOURCE_H

#include <string>
#include <utility>
#include <memory>
#include <boost/system/error_code.hpp>
#include "frame_buffer.h"
#include "rtmp_demuxer.h"
#include "execution.h"
#include "channel.h"
#include "sink.h"

namespace simple_rtmp
{

class rtmp_source : public std::enable_shared_from_this<rtmp_source>
{
   public:
    using prt = std::shared_ptr<rtmp_source>;

    explicit rtmp_source(std::string id, simple_rtmp::executors::executor& ex);

   public:
    void write(const frame_buffer::ptr& frame, const boost::system::error_code& ec);
    void on_codec(int codec);
    void set_channel(const channel::ptr& ch);

   private:
    void on_frame(const frame_buffer::ptr& frame, const boost::system::error_code& ec);

   private:
    std::string id_;
    channel::ptr ch_;
    sink::ptr rtmp_sink_;
    rtmp_demuxer::prt demuxer_;
};

}    // namespace simple_rtmp

#endif
