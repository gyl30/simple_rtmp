#include "rtsp_h264_encoder.h"

using simple_rtmp::rtsp_h264_encoder;

rtsp_h264_encoder::rtsp_h264_encoder(std::string id) : id_(std::move(id))
{
}

std::string simple_rtmp::rtsp_h264_encoder::id()
{
    return id_;
}

static const uint8_t* h264_startcode(const uint8_t* data, size_t bytes)
{
    size_t i;
    for (i = 2; i + 1 < bytes; i++)
    {
        if (0x01 == data[i] && 0x00 == data[i - 1] && 0x00 == data[i - 2])
            return data + i + 1;
    }

    return nullptr;
}

void rtsp_h264_encoder::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    // clang-format off
    enum { NAL_NIDR = 1, NAL_PARTITION_A = 2, NAL_IDR = 5, NAL_SEI = 6, NAL_SPS = 7, NAL_PPS = 8, NAL_AUD = 9, };
    // clang-format on

    const uint8_t* data = frame->data();
    uint32_t bytes = frame->size();
    const uint8_t* end = data + bytes;
    const uint8_t* p = h264_startcode(data, bytes);
    uint32_t n = 0;
    while (p)
    {
        const uint8_t* next = h264_startcode(p, static_cast<int>(end - p));
        if (next)
        {
            n = next - p - 3;
        }
        else
        {
            n = end - p;
        }

        while (n > 0 && 0 == p[n - 1]) n--;    // filter tailing zero

        assert(n > 0);
        if (n > 0)
        {
            uint8_t nalu_type = p[0] & 0x1f;
            if (nalu_type == NAL_SPS)
            {
                sps_ = fixed_frame_buffer::create(p, n);
            }
            if (nalu_type == NAL_PPS)
            {
                pps_ = fixed_frame_buffer::create(p, n);
            }
            // handler(param, p, (int)n);
        }

        p = next;
    }

    // encode frame
}

void rtsp_h264_encoder::set_output(const simple_rtmp::channel::ptr& ch)
{
    ch_ = ch;
}

simple_rtmp::rtsp_track::ptr rtsp_h264_encoder::track()
{
    if (sps_ && pps_)
    {
        return std::make_shared<rtsp_h264_track>(sps_, pps_);
    }
    return nullptr;
}
