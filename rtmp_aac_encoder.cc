#include "rtmp_aac_encoder.h"
#include "rtmp_codec.h"
#include "flv-header.h"
#include "mpeg4-aac.h"

enum
{
    sequence_header = 0,
    avpacket        = 1,
    end_of_sequence = 2,
};

using simple_rtmp::rtmp_aac_encoder;

struct rtmp_aac_encoder::args
{
    struct mpeg4_aac_t aac;
    uint8_t audio_sequence_header;
};

rtmp_aac_encoder::rtmp_aac_encoder(std::string id) : id_(std::move(id)), args_(std::make_shared<rtmp_aac_encoder::args>())
{
}

std::string rtmp_aac_encoder::id()
{
    return id_;
}

void rtmp_aac_encoder::set_output(const channel::ptr &ch)
{
    ch_ = ch;
}

void rtmp_aac_encoder::on_frame(const frame_buffer::ptr &frame, boost::system::error_code ec)
{
    if (ch_)
    {
        ch_->write(frame, ec);
    }
}

void rtmp_aac_encoder::write(const frame_buffer::ptr &frame, const boost::system::error_code &ec)
{
    if (ec)
    {
        on_frame(frame, ec);
        return;
    }
    int n = mpeg4_aac_adts_load(frame->data(), frame->size(), &args_->aac);
    if (n <= 0)
    {
        return;
    }
    const static uint8_t kCodecId  = simple_rtmp::rtmp_codec::aac;
    const static uint8_t kFrameTag = simple_rtmp::rtmp_tag::audio;

    struct flv_audio_tag_header_t audio;

    audio.codecid  = simple_rtmp::rtmp_codec::aac;
    audio.rate     = 3;    // 44k-SoundRate
    audio.bits     = 1;    // 16-bit samples
    audio.channels = 1;    // Stereo sound
    if (args_->audio_sequence_header == 0)
    {
        auto aac_frame = fixed_frame_buffer::create();
        aac_frame->set_pts(frame->pts());
        aac_frame->set_dts(frame->dts());
        aac_frame->set_codec(kCodecId);
        aac_frame->set_media(kFrameTag);
        aac_frame->resize(frame->size() + 4);

        audio.avpacket               = sequence_header;
        args_->audio_sequence_header = 1;    // once only
        flv_audio_tag_header_write(&audio, aac_frame->data(), aac_frame->size());
        n = mpeg4_aac_audio_specific_config_save(&args_->aac, aac_frame->data() + 2, aac_frame->size() - 2);
        aac_frame->resize(n + 2);
        on_frame(aac_frame, {});
    }
    audio.avpacket = avpacket;
    auto aac_frame = fixed_frame_buffer::create();
    aac_frame->set_pts(frame->pts());
    aac_frame->set_dts(frame->dts());
    aac_frame->set_codec(kCodecId);
    aac_frame->set_media(kFrameTag);
    aac_frame->resize(frame->size() + 4);
    int m = flv_audio_tag_header_write(&audio, aac_frame->data(), aac_frame->size());
    if (m == -1)
    {
        return;
    }
    aac_frame->resize(m);
    aac_frame->append(frame->data() + n, frame->size() - n);
    on_frame(aac_frame, {});
}
