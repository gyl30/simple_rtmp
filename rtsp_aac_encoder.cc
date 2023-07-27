#include "rtsp_aac_encoder.h"
#include "rtmp_codec.h"
#include "rtsp_aac_track.h"
#include "timestamp.h"

extern "C"
{
#include "rtp-payload.h"
#include "rtp-profile.h"
#include "rtp.h"
#include "mpeg4-aac.h"
}

using simple_rtmp::rtsp_aac_encoder;

static void* rtp_alloc(void* param, int bytes);
static void rtp_free(void* param, void* packet);
static void on_rtcp_event(void* param, const struct rtcp_msg_t* msg);

rtsp_aac_encoder::rtsp_aac_encoder(std::string id) : id_(std::move(id))
{
}
std::string simple_rtmp::rtsp_aac_encoder::id()
{
    return id_;
}
simple_rtmp::rtsp_track::ptr simple_rtmp::rtsp_aac_encoder::track()
{
    return track_;
}

void simple_rtmp::rtsp_aac_encoder::set_output(const simple_rtmp::channel::ptr& ch)
{
    ch_ = ch;
}

int rtsp_aac_encoder::rtp_encode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    auto* self = static_cast<rtsp_aac_encoder*>(param);
    auto frame = fixed_frame_buffer::create(packet, bytes);
    frame->set_pts(timestamp);
    frame->set_dts(timestamp);
    frame->set_media(simple_rtmp::rtmp_tag::audio);
    frame->set_flag(flags);
    self->ch_->write(frame, {});
    return 0;
}
void rtsp_aac_encoder::send_rtcp(const void* packet, int bytes, uint32_t timestamp, int flags)
{
    if (rtcp_ == nullptr)
    {
        struct rtp_event_t event;
        event.on_rtcp = on_rtcp_event;
        rtcp_ = rtp_create(&event, this, track_->ssrc(), timestamp, sample_rate_, 4 * 1024, 1);
        rtp_set_info(rtcp_, "SimpleRtsp", "ux");
    }

    auto now = timestamp::now().seconds();

    if (rtcp_timestamp_ != 0 && rtcp_timestamp_ + 5 > now)
    {
        return;
    }
    rtcp_timestamp_ = now;
    char buffer[1024] = {0};
    size_t n = rtp_rtcp_report(rtcp_, buffer, sizeof(buffer));
    auto frame = fixed_frame_buffer::create(buffer, n);
    frame->set_pts(0);
    frame->set_dts(0);
    frame->set_media(simple_rtmp::rtmp_tag::script);
    frame->set_flag(kRtcpAudioChannel);
    ch_->write(frame, {});
    rtp_onsend(rtcp_, packet, bytes /*, time*/);
}
void simple_rtmp::rtsp_aac_encoder::write(const simple_rtmp::frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        ch_->write(nullptr, ec);
        return;
    }
    if (track_ == nullptr)
    {
        struct mpeg4_aac_t aac;
        memset(&aac, 0, sizeof(aac));
        if (mpeg4_aac_adts_load(frame->data(), frame->size(), &aac) < 0)
        {
            return;
        }
        char buf[32] = {0};
        int len = mpeg4_aac_audio_specific_config_save(&aac, reinterpret_cast<uint8_t*>(buf), sizeof(buf));
        if (len < 0)
        {
            return;
        }
        sample_rate_ = mpeg4_aac_audio_frequency_to(static_cast<enum mpeg4_aac_frequency>(aac.sampling_frequency_index));
        channel_count_ = aac.channel_configuration;
        track_ = std::make_shared<simple_rtmp::rtsp_aac_track>(std::string(buf, len), sample_rate_, channel_count_, 0);
    }
    if (ctx_ == nullptr && track_ != nullptr)
    {
        struct rtp_payload_t handler;
        handler.alloc = rtp_alloc;
        handler.free = rtp_free;
        handler.packet = rtp_encode_packet;
        ctx_ = rtp_payload_encode_create(98, "mpeg4-generic", 0, track_->ssrc(), &handler, this);
    }
    if (ctx_ == nullptr)
    {
        return;
    }
    rtp_payload_encode_input(ctx_, frame->data(), static_cast<int>(frame->size()), frame->pts());
}
static void* rtp_alloc(void* /*param*/, int bytes)
{
    return malloc(bytes);
}

static void rtp_free(void* /*param*/, void* packet)
{
    free(packet);
}

static void on_rtcp_event(void* param, const struct rtcp_msg_t* msg)
{
    (void)param;
    (void)msg;
}
