#include <utility>
#include "rtmp_hevc_decoder.h"
#include "rtmp_codec.h"
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

#define H265_NAL_BLA_W_LP 16
#define H265_NAL_RSV_IRAP 23
#define H265_NAL_VPS 32
#define H265_NAL_SPS 33
#define H265_NAL_PPS 34
#define H265_NAL_AUD 35    // Access unit delimiter

using simple_rtmp::rtmp_h265_decoder;

struct rtmp_h265_decoder::args
{
    struct mpeg4_hevc_t hevc;
    int vps_sps_pps_flag = 0;
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

static int h265_vps_sps_pps_size(const struct mpeg4_hevc_t* hevc)
{
    int i, n = 0;
    for (i = 0; i < hevc->numOfArrays; i++) n += hevc->nalu[i].bytes + 4;
    return n;
}

void rtmp_h265_decoder::demuxer_avpacket(const uint8_t* data, size_t bytes, int64_t timestamp, int64_t cts, int keyframe)
{
    size_t offset = 0;
    const uint8_t* data_offset = data;
    const uint8_t* end = data + bytes;

    while (true)
    {
        int length = 0;
        for (int i = 0; i < args_->hevc.lengthSizeMinusOne + 1; i++)
        {
            length = (length << 8) + (const_cast<uint8_t*>(data_offset))[i];
        }
        if (args_->hevc.lengthSizeMinusOne == 0 || (length == 1 && (args_->hevc.lengthSizeMinusOne == 2 || args_->hevc.lengthSizeMinusOne == 3)))
        {
            // annexb
            break;
        }
        offset += args_->hevc.lengthSizeMinusOne + 1;
        if (length < 0 || data_offset + length > end)
        {
            break;
        }
        int nalu_type = (data_offset[offset] >> 1) & 0x3f;
        if (H265_NAL_VPS == nalu_type || H265_NAL_SPS == nalu_type || H265_NAL_PPS == nalu_type)
        {
            args_->vps_sps_pps_flag = 1;
        }
        int irap = static_cast<int>((H265_NAL_BLA_W_LP <= nalu_type) && (nalu_type <= H265_NAL_RSV_IRAP));
        if (irap && args_->vps_sps_pps_flag == 0)
        {
            int n = h265_vps_sps_pps_size(&args_->hevc);
            auto frame = fixed_frame_buffer::create(data_offset, n);
            frame->set_media(simple_rtmp::rtmp_tag::video);
            frame->set_codec(simple_rtmp::rtmp_codec::h265);
            frame->set_flag(keyframe);
            frame->set_pts(timestamp + cts);
            frame->set_dts(timestamp);
            n = mpeg4_hevc_to_nalu(&args_->hevc, frame->data(), frame->size());
            if (n <= 0)
            {
                return;
            }
            frame->resize(n);
            offset += n;
            on_frame(frame, {});
            args_->vps_sps_pps_flag = 1;
        }
        static const uint8_t h265_start_code[] = {0x00, 0x00, 0x00, 0x01};
        data_offset += offset;
        auto frame = fixed_frame_buffer::create(data_offset, length);
        frame->set_media(simple_rtmp::rtmp_tag::video);
        frame->set_codec(simple_rtmp::rtmp_codec::h265);
        frame->set_flag(keyframe);
        frame->set_pts(timestamp + cts);
        frame->set_dts(timestamp);
        on_frame(frame, {});

        offset += length;
        data_offset += offset;
    }
}
