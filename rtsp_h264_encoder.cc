#include <cassert>
#include "rtsp_h264_encoder.h"
#include "rtmp_codec.h"
#include "timestamp.h"
extern "C"
{
#include "rtp-payload.h"
#include "rtp-profile.h"
#include "rtp.h"
}

using simple_rtmp::rtsp_h264_encoder;

static const auto kHz = 90;    // 90KHz

static void* rtp_alloc(void* param, int bytes);
static void rtp_free(void* param, void* packet);
static const uint8_t* h264_startcode(const uint8_t* data, size_t bytes);

rtsp_h264_encoder::rtsp_h264_encoder(std::string id) : id_(std::move(id))
{
}

std::string simple_rtmp::rtsp_h264_encoder::id()
{
    return id_;
}
void rtsp_h264_encoder::set_output(const simple_rtmp::channel::ptr& ch)
{
    ch_ = ch;
}

simple_rtmp::rtsp_track::ptr rtsp_h264_encoder::track()
{
    return track_;
}

int rtsp_h264_encoder::rtp_encode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    auto* self = static_cast<rtsp_h264_encoder*>(param);
    auto frame = fixed_frame_buffer::create(packet, bytes);
    frame->set_pts(timestamp);
    frame->set_dts(timestamp);
    frame->set_media(simple_rtmp::rtmp_tag::video);
    frame->set_flag(flags);
    self->ch_->write(frame, {});
    return 0;
}

void rtsp_h264_encoder::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        ch_->write(frame, ec);
        return;
    }

    // clang-format off
    enum { NAL_NIDR = 1, NAL_PARTITION_A = 2, NAL_IDR = 5, NAL_SEI = 6, NAL_SPS = 7, NAL_PPS = 8, NAL_AUD = 9, };
    // clang-format on

    const uint8_t* data = frame->data();
    uint32_t bytes = frame->size();
    const uint8_t* end = data + bytes;
    const uint8_t* p = h264_startcode(data, bytes);
    uint32_t n = 0;
    while (p != nullptr)
    {
        const uint8_t* next = h264_startcode(p, static_cast<int>(end - p));
        if (next != nullptr)
        {
            n = next - p - 3;
        }
        else
        {
            n = end - p;
        }

        while (n > 0 && 0 == p[n - 1])
        {
            n--;
        }

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
        }

        p = next;
    }

    if (track_ == nullptr && sps_ && pps_)
    {
        track_ = std::make_shared<rtsp_h264_track>(sps_, pps_);
    }
    if (ctx_ == nullptr && track_ != nullptr)
    {
        struct rtp_payload_t handler;
        handler.alloc = rtp_alloc;
        handler.free = rtp_free;
        handler.packet = rtp_encode_packet;
        ctx_ = rtp_payload_encode_create(96, "H264", 0, track_->ssrc(), &handler, this);
    }
    if (ctx_ == nullptr)
    {
        return;
    }
    rtp_payload_encode_input(ctx_, frame->data(), static_cast<int>(frame->size()), frame->pts() * kHz);
}

static const uint8_t* h264_startcode(const uint8_t* data, size_t bytes)
{
    size_t i;
    for (i = 2; i + 1 < bytes; i++)
    {
        if (0x01 == data[i] && 0x00 == data[i - 1] && 0x00 == data[i - 2])
        {
            return data + i + 1;
        }
    }

    return nullptr;
}

static void* rtp_alloc(void* /*param*/, int bytes)
{
    return malloc(bytes);
}

static void rtp_free(void* /*param*/, void* packet)
{
    free(packet);
}
