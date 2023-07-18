#include "rtsp_aac_encoder.h"
#include "rtmp_codec.h"
#include "rtsp_aac_track.h"

extern "C"
{
#include "rtp-payload.h"
#include "mpeg4-aac.h"
}

using simple_rtmp::rtsp_aac_encoder;

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

static void* rtp_alloc(void* /*param*/, int bytes)
{
    return malloc(bytes);
}

static void rtp_free(void* /*param*/, void* packet)
{
    free(packet);
}

int rtsp_aac_encoder::rtp_encode_packet(void* param, const void* packet, int bytes, uint32_t timestamp, int flags)
{
    auto* self = static_cast<rtsp_aac_encoder*>(param);
    auto frame = fixed_frame_buffer::create();
    frame->append(packet, bytes);
    frame->set_pts(timestamp);
    frame->set_dts(timestamp);
    frame->set_media(simple_rtmp::rtmp_tag::audio);
    frame->set_codec(flags);
    self->ch_->write(frame, {});
    return 0;
}

void simple_rtmp::rtsp_aac_encoder::write(const simple_rtmp::frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (specific_config_ == nullptr)
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
        int sample_rate = mpeg4_aac_audio_frequency_to(static_cast<enum mpeg4_aac_frequency>(aac.sampling_frequency_index));
        int channel_count = aac.channel_configuration;
        char buf[32] = {0};
        len = mpeg4_aac_audio_specific_config_save(&aac, (uint8_t*)buf, sizeof(buf));
        if (len > 0)
        {
            track_ = std::make_shared<simple_rtmp::rtsp_aac_track>(std::string(buf, len), sample_rate, channel_count, 0);
        }
        specific_config_ = fixed_frame_buffer::create(buf, len);
    }
}
