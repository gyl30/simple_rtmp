#include <cstring>
#include "flv-demuxer.h"
#include "rtmp_demuxer.h"
#include "rtmp_codec.h"
#include "rtmp_h264_decoder.h"
#include "rtmp_aac_decoder.h"
#include "amf0.h"
#include "log.h"

using simple_rtmp::rtmp_demuxer;

rtmp_demuxer::rtmp_demuxer(std::string& id) : id_(id)
{
}

void rtmp_demuxer::set_channel(const channel::ptr& ch)
{
    ch_ = ch;
}

void rtmp_demuxer::on_codec(const std::function<void(int)>& codec_cb)
{
    codec_cb_ = codec_cb;
}

void rtmp_demuxer::write(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        demuxer_script(frame, ec);
        demuxer_video(frame, ec);
        demuxer_audio(frame, ec);
        return;
    }
    if (frame->codec == simple_rtmp::rtmp_tag::script)
    {
        demuxer_script(frame, ec);
    }
    else if (frame->codec == simple_rtmp::rtmp_tag::video)
    {
        demuxer_video(frame, ec);
    }
    else if (frame->codec == simple_rtmp::rtmp_tag::audio)
    {
        demuxer_audio(frame, ec);
    }
}

void rtmp_demuxer::demuxer_video(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (video_decoder_)
    {
        video_decoder_->write(frame, ec);
    }
}

void rtmp_demuxer::demuxer_audio(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (audio_decoder_)
    {
        audio_decoder_->write(frame, ec);
    }
}
void rtmp_demuxer::demuxer_script(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        return;
    }
    const uint8_t* data = frame->payload.data();
    size_t bytes        = frame->payload.size();
    const uint8_t* end;
    char buffer[64]        = {0};
    double audiocodecid    = 0;
    double audiodatarate   = 0;    // bitrate / 1024
    double audiodelay      = 0;
    double audiosamplerate = 0;
    double audiosamplesize = 0;
    double videocodecid    = 0;
    double videodatarate   = 0;    // bitrate / 1024
    double framerate       = 0;
    double height          = 0;
    double width           = 0;
    double duration        = 0;
    double filesize        = 0;
    int canSeekToEnd       = 0;
    int stereo             = 0;
    struct amf_object_item_t keyframes[2];
    struct amf_object_item_t prop[16];
    struct amf_object_item_t items[1];

#define AMF_OBJECT_ITEM_VALUE(v, amf_type, amf_name, amf_value, amf_size) \
    {                                                                     \
        v.type  = amf_type;                                               \
        v.name  = amf_name;                                               \
        v.value = amf_value;                                              \
        v.size  = amf_size;                                               \
    }
    AMF_OBJECT_ITEM_VALUE(keyframes[0], AMF_STRICT_ARRAY, "filepositions", NULL, 0);    // ignore keyframes
    AMF_OBJECT_ITEM_VALUE(keyframes[1], AMF_STRICT_ARRAY, "times", nullptr, 0);

    AMF_OBJECT_ITEM_VALUE(prop[0], AMF_NUMBER, "audiocodecid", &audiocodecid, sizeof(audiocodecid));
    AMF_OBJECT_ITEM_VALUE(prop[1], AMF_NUMBER, "audiodatarate", &audiodatarate, sizeof(audiodatarate));
    AMF_OBJECT_ITEM_VALUE(prop[2], AMF_NUMBER, "audiodelay", &audiodelay, sizeof(audiodelay));
    AMF_OBJECT_ITEM_VALUE(prop[3], AMF_NUMBER, "audiosamplerate", &audiosamplerate, sizeof(audiosamplerate));
    AMF_OBJECT_ITEM_VALUE(prop[4], AMF_NUMBER, "audiosamplesize", &audiosamplesize, sizeof(audiosamplesize));
    AMF_OBJECT_ITEM_VALUE(prop[5], AMF_BOOLEAN, "stereo", &stereo, sizeof(stereo));

    AMF_OBJECT_ITEM_VALUE(prop[6], AMF_BOOLEAN, "canSeekToEnd", &canSeekToEnd, sizeof(canSeekToEnd));
    AMF_OBJECT_ITEM_VALUE(prop[7], AMF_STRING, "creationdate", buffer, sizeof(buffer));
    AMF_OBJECT_ITEM_VALUE(prop[8], AMF_NUMBER, "duration", &duration, sizeof(duration));
    AMF_OBJECT_ITEM_VALUE(prop[9], AMF_NUMBER, "filesize", &filesize, sizeof(filesize));

    AMF_OBJECT_ITEM_VALUE(prop[10], AMF_NUMBER, "videocodecid", &videocodecid, sizeof(videocodecid));
    AMF_OBJECT_ITEM_VALUE(prop[11], AMF_NUMBER, "videodatarate", &videodatarate, sizeof(videodatarate));
    AMF_OBJECT_ITEM_VALUE(prop[12], AMF_NUMBER, "framerate", &framerate, sizeof(framerate));
    AMF_OBJECT_ITEM_VALUE(prop[13], AMF_NUMBER, "height", &height, sizeof(height));
    AMF_OBJECT_ITEM_VALUE(prop[14], AMF_NUMBER, "width", &width, sizeof(width));

    AMF_OBJECT_ITEM_VALUE(prop[15], AMF_OBJECT, "keyframes", keyframes, 2);    // FLV I-index

    AMF_OBJECT_ITEM_VALUE(items[0], AMF_OBJECT, "onMetaData", prop, sizeof(prop) / sizeof(prop[0]));
#undef AMF_OBJECT_ITEM_VALUE

    end = data + bytes;
    if (data[0] != AMF_STRING)
    {
        return;
    }
    data = AMFReadString(data + 1, end, 0, buffer, sizeof(buffer) - 1);
    if (data == nullptr)
    {
        return;
    }
    LOG_DEBUG("{} amf read {}", id_, buffer);

    // filter @setDataFrame
    if (strcmp(buffer, "@setDataFrame") == 0)
    {
        if (data[0] != AMF_STRING)
        {
            return;
        }
        data = AMFReadString(data + 1, end, 0, buffer, sizeof(buffer) - 1);
        if (data == nullptr)
        {
            return;
        }
        LOG_DEBUG("{} amf read {}", id_, buffer);
    }

    // onTextData/onCaption/onCaptionInfo/onCuePoint/|RtmpSampleAccess
    if (strcmp(buffer, "onMetaData") != 0)
    {
        return;    // skip
    }

    data = amf_read_items(data, end, items, sizeof(items) / sizeof(items[0]));
    if (data == nullptr)
    {
        LOG_WARN("{} demuxer script failed", id_);
    }
    LOG_DEBUG("{} onMetaData audiocodecid {} audiodelay {} audiodatarate {} audiosamplesize {} videocodecid {} videodatarate {} framerate {} width {} height {} duration {} filesize {}",
              id_,
              audiocodecid,
              audiodelay,
              audiodatarate,
              audiosamplesize,
              videocodecid,
              videodatarate,
              framerate,
              width,
              height,
              duration,
              filesize);
    if (codec_cb_)
    {
        codec_cb_(videocodecid);
        codec_cb_(audiocodecid);
    }
    int64_t audio_codec_id = audiocodecid;
    audio_codec_id         = audio_codec_id << 4;
    if (videocodecid == simple_rtmp::rtmp_codec::h264)
    {
        video_decoder_ = std::make_shared<rtmp_h264_decoder>(id_);
    }
    if (audio_codec_id == simple_rtmp::rtmp_codec::aac)
    {
        audio_decoder_ = std::make_shared<rtmp_aac_decoder>(id_);
    }
    if (video_decoder_)
    {
        video_decoder_->set_output(ch_);
    }
    if (audio_decoder_)
    {
        audio_decoder_->set_output(ch_);
    }
}
