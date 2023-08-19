#include <cassert>
#include "rtsp_hevc_encoder.h"
#include "rtmp_codec.h"
#include "timestamp.h"
extern "C"
{
#include "rtp-payload.h"
#include "rtp-profile.h"
#include "rtp.h"
}

using simple_rtmp::rtsp_hevc_encoder;

static const auto kHz = 90;    // 90KHz

static void* rtp_alloc(void* param, int bytes);
static void rtp_free(void* param, void* packet);

rtsp_hevc_encoder::rtsp_hevc_encoder(std::string id) : id_(std::move(id))
{
}

std::string simple_rtmp::rtsp_hevc_encoder::id()
{
    return id_;
}
void rtsp_hevc_encoder::set_output(const simple_rtmp::channel::ptr& ch)
{
    ch_ = ch;
}

simple_rtmp::rtsp_track::ptr rtsp_hevc_encoder::track()
{
    return track_;
}

int rtsp_hevc_encoder::rtp_encode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    auto* self = static_cast<rtsp_hevc_encoder*>(param);
    auto frame = fixed_frame_buffer::create(packet, bytes);
    frame->set_pts(timestamp);
    frame->set_dts(timestamp);
    frame->set_media(simple_rtmp::rtmp_tag::video);
    frame->set_flag(flags);
    self->ch_->write(frame, {});
    return 0;
}

void rtsp_hevc_encoder::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        ch_->write(frame, ec);
        return;
    }
}

static void* rtp_alloc(void* /*param*/, int bytes)
{
    return malloc(bytes);
}

static void rtp_free(void* /*param*/, void* packet)
{
    free(packet);
}
