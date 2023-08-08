#include <utility>
#include "rtmp_hevc_encoder.h"
#include "mpeg4-hevc.h"
#include "rtmp_codec.h"
#include "log.h"

enum
{
    sequence_header = 0,
    avpacket = 1,
    end_of_sequence = 2,
};

using simple_rtmp::rtmp_hevc_encoder;

struct rtmp_hevc_encoder::args
{
    struct mpeg4_hevc_t hevc;
};

rtmp_hevc_encoder::rtmp_hevc_encoder(std::string id) : id_(std::move(id)), args_(std::make_shared<rtmp_hevc_encoder::args>())
{
}

std::string rtmp_hevc_encoder::id()
{
    return id_;
}

void rtmp_hevc_encoder::set_output(const channel::ptr &ch)
{
    ch_ = ch;
}

void rtmp_hevc_encoder::on_frame(const frame_buffer::ptr &frame, boost::system::error_code ec)
{
    if (ch_)
    {
        ch_->write(frame, ec);
    }
}

void rtmp_hevc_encoder::write(const frame_buffer::ptr &frame, const boost::system::error_code &ec)
{
    if (ec)
    {
        on_frame(frame, ec);
        return;
    }
}
