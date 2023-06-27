#include <utility>
#include "rtmp_aac_decoder.h"
#include "flv-header.h"
#include "mpeg4-aac.h"
#include "rtmp_codec.h"
#include "log.h"

enum
{
    sequence_header = 0,
    avpacket = 1,
    end_of_sequence = 2,
};

using simple_rtmp::rtmp_aac_decoder;

struct rtmp_aac_decoder::args
{
    struct mpeg4_aac_t aac;
};

rtmp_aac_decoder::rtmp_aac_decoder(std::string id) : id_(std::move(id))
{
    args_ = std::make_shared<args>();
}

rtmp_aac_decoder::~rtmp_aac_decoder() = default;

void rtmp_aac_decoder::set_output(const simple_rtmp::channel::ptr& ch)
{
    ch_ = ch;
}

void rtmp_aac_decoder::on_frame(const frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ch_)
    {
        ch_->write(frame, ec);
    }
}

void rtmp_aac_decoder::write(const frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ec)
    {
        on_frame(frame, ec);
        return;
    }

    const uint8_t* data = frame->data();
    size_t const bytes = frame->size();

    struct flv_audio_tag_header_t audio;
    int const n = flv_audio_tag_header_read(&audio, data, bytes);
    if (sequence_header == audio.avpacket)
    {
        args_->aac.profile = MPEG4_AAC_LC;
        args_->aac.sampling_frequency_index = MPEG4_AAC_44100;
        args_->aac.channel_configuration = 2;
        args_->aac.channels = 2;
        args_->aac.sampling_frequency = 44100;
        args_->aac.extension_frequency = 44100;
        mpeg4_aac_audio_specific_config_load(data + n, bytes - n, &args_->aac);
        return;
    }
    auto aac_frame = fixed_frame_buffer::create();
    aac_frame->resize(args_->aac.npce + 7 + 1);
    aac_frame->set_media(simple_rtmp::rtmp_tag::audio);
    aac_frame->set_codec(simple_rtmp::rtmp_codec::aac);
    aac_frame->set_pts(frame->pts());
    aac_frame->set_dts(frame->dts());
    int const ret = mpeg4_aac_adts_save(&args_->aac, static_cast<uint16_t>(bytes - n), aac_frame->data(), aac_frame->size());
    if (ret < 7)
    {
        return;
    }
    args_->aac.npce = 0;
    aac_frame->resize(ret);
    aac_frame->append(data + n, bytes - n);
    on_frame(aac_frame, {});
}
