#ifndef SIMPLE_RTMP_GB28181_RTP_DEMUXER_H
#define SIMPLE_RTMP_GB28181_RTP_DEMUXER_H

#include <memory>
#include <string>
#include <boost/system/error_code.hpp>
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
    frame_buffer::ptr write(const frame_buffer::ptr& frame);

   private:
    std::string id_;
    std::vector<uint8_t> data_;
};

}    // namespace simple_rtmp

#endif
