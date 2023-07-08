#include <utility>
#include "rtmp_h264_decoder.h"
#include "flv-header.h"
#include "mpeg4-avc.h"
#include "rtmp_codec.h"
#include "log.h"
#include "execution.h"
#include <deque>

enum
{
    sequence_header = 0,
    avpacket = 1,
    end_of_sequence = 2,
};

using simple_rtmp::rtmp_h264_decoder;

struct rtmp_h264_decoder::args
{
    struct mpeg4_avc_t avc;
    int sps_pps_flag = 0;
};

rtmp_h264_decoder::rtmp_h264_decoder(std::string id) : id_(std::move(id))
{
}

rtmp_h264_decoder::~rtmp_h264_decoder() = default;

void rtmp_h264_decoder::set_output(const channel::ptr& ch)
{
    ch_ = ch;
}

void rtmp_h264_decoder::on_frame(const frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ch_)
    {
        ch_->write(frame, ec);
    }
}

void rtmp_h264_decoder::write(const frame_buffer::ptr& frame, boost::system::error_code ec)
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
        mpeg4_avc_decoder_configuration_record_load(data, bytes, &args_->avc);
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

static int h264_sps_pps_size(const struct mpeg4_avc_t* avc)
{
    int n = 0;
    for (int i = 0; i < avc->nb_sps; i++)
    {
        n += avc->sps[i].bytes + 4;
    }
    for (int i = 0; i < avc->nb_pps; i++)
    {
        n += avc->pps[i].bytes + 4;
    }
    return n;
}

void rtmp_h264_decoder::demuxer_avpacket(const uint8_t* data, size_t bytes, int64_t timestamp, int64_t cts, int keyframe)
{
    size_t offset = 0;
    const uint8_t* data_end = data + bytes;
    while (true)
    {
        const uint8_t* data_offset = data + offset;
        if (data_offset >= data_end)
        {
            break;
        }
        LOG_TRACE("{} video avpacket {} bytes", id_, bytes);
        int nalu_size = 0;
        for (int i = 0; i < args_->avc.nalu; i++)
        {
            nalu_size = (nalu_size << 8) + data_offset[i];
        }
        LOG_TRACE("{} video avpacket {} bytes nalu size {}", id_, bytes, nalu_size);
        if (nalu_size == 0)
        {
            break;
        }

        int nalu_type = data_offset[args_->avc.nalu] & 0x1f;
        LOG_TRACE("{} video avpacket {} bytes nalu size {} nalu type {}", id_, bytes, nalu_size, nalu_type);
        if (nalu_type == 5 && args_->sps_pps_flag == 0)    // idr
        {
            int avc_length = h264_sps_pps_size(&args_->avc);
            auto frame = fixed_frame_buffer::create(avc_length);
            frame->set_media(simple_rtmp::rtmp_tag::video);
            frame->set_codec(simple_rtmp::rtmp_codec::h264);
            frame->set_flag(keyframe);
            frame->set_pts(timestamp + cts);
            frame->set_dts(timestamp);
            frame->resize(avc_length);
            int n = mpeg4_avc_to_nalu(&args_->avc, frame->data(), frame->size());
            if (n <= 0)
            {
                return;
            }
            on_frame(frame, {});
            args_->sps_pps_flag = 1;
        }

        const static uint8_t header[] = {0x00, 0x00, 0x00, 0x01};
        auto frame = fixed_frame_buffer::create(nalu_size + 4);
        frame->set_media(simple_rtmp::rtmp_tag::video);
        frame->set_codec(simple_rtmp::rtmp_codec::h264);
        frame->set_pts(timestamp + cts);
        frame->set_dts(timestamp);
        frame->set_flag(keyframe);
        frame->append(header, sizeof header);
        frame->append(data_offset + args_->avc.nalu, nalu_size);
        on_frame(frame, {});
        offset = offset + args_->avc.nalu + nalu_size;
    }
    assert(data + offset == data_end);
}
