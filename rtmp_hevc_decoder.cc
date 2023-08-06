#include <utility>
#include "rtmp_hevc_decoder.h"
#include "flv-header.h"
#include "mpeg4-hevc.h"
#include "execution.h"
#include "log.h"

enum
{
    sequence_header = 0,
    avpacket = 1,
    end_of_sequence = 2,
};

using simple_rtmp::rtmp_h265_decoder;

struct rtmp_h265_decoder::args
{
    struct mpeg4_hevc_t hevc;
};

rtmp_h265_decoder::rtmp_h265_decoder(std::string id) : id_(std::move(id))
{
}

rtmp_h265_decoder::~rtmp_h265_decoder() = default;

void rtmp_h265_decoder::set_output(const channel::ptr& ch)
{
    ch_ = ch;
}

void rtmp_h265_decoder::on_frame(const frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ch_)
    {
        ch_->write(frame, ec);
    }
}

void rtmp_h265_decoder::write(const frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ec)
    {
        on_frame(frame, ec);
        return;
    }

    const uint8_t* data = frame->data();
    size_t bytes = frame->size();

    struct flv_video_tag_header_t video;
    int n = flv_video_tag_header_read(&video, data, bytes);
    if (n < 0)
    {
        return;
    }
    data += n;
    bytes -= n;
    if (video.avpacket == sequence_header)
    {
        LOG_TRACE("{} video sequence header {} bytes", id_, bytes);
        args_ = std::make_shared<struct args>();
        mpeg4_hevc_decoder_configuration_record_load(data, bytes, &args_->hevc);
        return;
    }
    if (args_ == nullptr)
    {
        return;
    }

    if (video.avpacket == end_of_sequence)
    {
        on_frame({}, boost::asio::error::eof);
        return;
    }
    if (video.avpacket != avpacket)
    {
        return;
    }
    demuxer_avpacket(data, bytes, frame->pts(), video.cts, video.keyframe == 1 ? 1 : 0);
}

void rtmp_h265_decoder::demuxer_avpacket(const uint8_t* data, size_t bytes, int64_t timestamp, int64_t cts, int keyframe)
{
}
