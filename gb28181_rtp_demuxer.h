#ifndef SIMPLE_RTMP_GB28181_RTP_DEMUXER_H
#define SIMPLE_RTMP_GB28181_RTP_DEMUXER_H

#include <memory>
#include <string>
#include <boost/system/error_code.hpp>
#include "rtp-queue.h"
#include "channel.h"
#include "frame_buffer.h"

namespace simple_rtmp
{
class gb28181_rtp_demuxer : public std::enable_shared_from_this<gb28181_rtp_demuxer>
{
   public:
    using prt = std::shared_ptr<gb28181_rtp_demuxer>;

   public:
    explicit gb28181_rtp_demuxer(std::string& id);

   public:
    void write(const frame_buffer::ptr& frame);
    void set_channel(const channel::ptr& ch);

   private:
    void on_frame(const frame_buffer::ptr& frame, boost::system::error_code ec);

   private:
    std::string id_;
    channel::ptr ch_;
    std::vector<uint8_t> data_;
    rtp_queue_t* queue_ = nullptr;
};

}    // namespace simple_rtmp

#endif
