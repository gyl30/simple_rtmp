#include "log.h"
#include "flv-header.h"
#include "flv-writer.h"
#include "mpeg4-avc.h"
#include "frame_buffer.h"
#include "flv_h264_encoder.h"
#include "rtmp_codec.h"

enum
{
    sequence_header = 0,
    avpacket = 1,
    end_of_sequence = 2,
};

#define N_TAG_SIZE 4              // previous tag size
#define FLV_HEADER_SIZE 9         // DataOffset included
#define FLV_TAG_HEADER_SIZE 11    // StreamID included

using simple_rtmp::flv_h264_encoder;

struct flv_h264_encoder::args
{
    struct mpeg4_avc_t avc;
    int vcl;       // 0-non vcl, 1-idr, 2-p/b
    int update;    // avc/hevc sequence header update
    uint8_t audio_sequence_header;
    uint8_t video_sequence_header;
};

flv_h264_encoder::flv_h264_encoder(std::string id) : id_(std::move(id))
{
}

std::string flv_h264_encoder::id()
{
    return id_;
};

void flv_h264_encoder::set_output(const channel::ptr& ch)
{
    //
    ch_ = ch;
}
static simple_rtmp::frame_buffer::ptr make_frame_header(const simple_rtmp::frame_buffer::ptr& frame)
{
    uint8_t buf[FLV_TAG_HEADER_SIZE + 4] = {0};
    struct flv_vec_t vec[3];
    struct flv_writer_t* flv;
    struct flv_tag_header_t tag;

    memset(&tag, 0, sizeof(tag));
    tag.size = static_cast<int>(frame->size());
    tag.type = static_cast<uint8_t>(frame->media());
    tag.timestamp = frame->dts();
    flv_tag_header_write(&tag, buf, FLV_TAG_HEADER_SIZE);
    flv_tag_size_write(buf + FLV_TAG_HEADER_SIZE, 4, frame->size() + FLV_TAG_HEADER_SIZE);
    return simple_rtmp::fixed_frame_buffer::create(buf, FLV_TAG_HEADER_SIZE);
}

void flv_h264_encoder::muxer_frame(const frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ch_)
    {
        auto header = make_frame_header(frame);
        ch_->write(header, ec);
        ch_->write(frame, ec);
    }
}

void flv_h264_encoder::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        muxer_frame(frame, ec);
        return;
    }

    const static uint8_t kCodecId = simple_rtmp::rtmp_codec::h264;
    const static uint8_t kFrameTag = simple_rtmp::rtmp_tag::video;
    const uint8_t* data = frame->data();
    size_t const size = frame->size();
    auto avc_frame = fixed_frame_buffer::create();
    avc_frame->resize(frame->size() + sizeof(args_->avc));

    int ret = h264_annexbtomp4(&args_->avc, data, size, avc_frame->data(), avc_frame->size(), &args_->vcl, &args_->update);
    if (ret <= 0)
    {
        return;
    }
    avc_frame->resize(ret);
    avc_frame->set_pts(frame->pts());
    avc_frame->set_dts(frame->dts());
    avc_frame->set_codec(kCodecId);
    avc_frame->set_media(kFrameTag);
    const static int kBufferSize = 4096;
    const static int kVideoTagSize = 5;

    if ((args_->update != 0) && args_->avc.nb_sps > 0 && args_->avc.nb_pps > 0)
    {
        uint8_t buf[kBufferSize] = {0};
        buf[0] = (1 << 4) | (kCodecId & 0x0F);
        buf[1] = sequence_header;
        buf[2] = (0 >> 16) & 0xFF;
        buf[3] = (0 >> 8) & 0xFF;
        buf[4] = 0 & 0xFF;

        ret = mpeg4_avc_decoder_configuration_record_save(&args_->avc, buf + kVideoTagSize, kBufferSize - kVideoTagSize);
        if (ret <= 0)
        {
            LOG_DEBUG("rtmp encoder configuration record save failed {}", ret);
            return;
        }
        auto config_frame = fixed_frame_buffer::create();
        config_frame->append(buf, ret + kVideoTagSize);
        config_frame->set_pts(frame->pts());
        config_frame->set_dts(frame->dts());
        config_frame->set_codec(kCodecId);
        config_frame->set_media(kFrameTag);
        config_frame->set_flag(1);
        args_->video_sequence_header = 1;
        muxer_frame(config_frame, {});
    }
    if ((args_->vcl != 0) && (args_->video_sequence_header != 0U))
    {
        uint8_t const keyframe = args_->vcl == 1 ? 1 : 2;
        uint8_t buf[kVideoTagSize] = {0};
        buf[0] = (keyframe << 4) | (kCodecId & 0x0F);
        buf[1] = avpacket;
        buf[2] = (0 >> 16) & 0xFF;
        buf[3] = (0 >> 8) & 0xFF;
        buf[4] = 0 & 0xFF;
        auto header_frame = fixed_frame_buffer::create();
        header_frame->append(buf, kVideoTagSize);
        header_frame->set_pts(frame->pts());
        header_frame->set_dts(frame->dts());
        header_frame->set_codec(kCodecId);
        header_frame->set_media(kFrameTag);
        auto encode_frame = fixed_frame_buffer::create();
        encode_frame->append(header_frame);
        encode_frame->append(avc_frame);
        encode_frame->set_flag(keyframe == 1 ? 1 : 0);
        muxer_frame(encode_frame, {});
    }
}
