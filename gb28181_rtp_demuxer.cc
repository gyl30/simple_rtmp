#include <rtp-packet.h>
#include "gb28181_rtp_demuxer.h"
using simple_rtmp::gb28181_rtp_demuxer;

void gb28181_rtp_demuxer::set_channel(const channel::ptr& ch)
{
    ch_ = ch;
}

void gb28181_rtp_demuxer::on_frame(const frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ch_)
    {
        ch_->write(frame, ec);
    }
}

void gb28181_rtp_demuxer::write(const frame_buffer::ptr& frame)
{
    constexpr auto kGB28181RtpTcpPrefixSize = 2;

    data_.insert(data_.end(), frame->data(), frame->data() + frame->size());

    if (data_.size() < kGB28181RtpTcpPrefixSize)
    {
        return;
    }
    uint16_t rtp_payload_size = (data_[0] << 8) | data_[1];
    if (data_.size() < rtp_payload_size + kGB28181RtpTcpPrefixSize)
    {
        return;
    }

    struct rtp_packet_t pkt;
    int ret = rtp_packet_deserialize(&pkt, data_.data() + kGB28181RtpTcpPrefixSize, rtp_payload_size);
    if (ret != 0)
    {
        on_frame(frame, boost::system::errc::make_error_code(boost::system::errc::protocol_error));
        return;
    }

    data_.clear();
}
