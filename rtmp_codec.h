#ifndef SIMPLE_RTMP_RTMP_CODEC_H
#define SIMPLE_RTMP_RTMP_CODEC_H

namespace simple_rtmp
{
enum rtmp_tag
{
    script,
    video,
    audio,
};
enum rtmp_codec
{
    // audio
    adpcm     = (1 << 4),
    mp3       = (2 << 4),
    g711a     = (7 << 4),
    g711u     = (8 << 4),
    aac       = (10 << 4),
    opus      = (13 << 4),
    mp3_8k    = (14 << 4),
    asc       = 0x100,
    opus_head = 0x101,
    // video
    h263 = 2,
    vp6  = 4,
    h264 = 7,
    h265 = 12,
    av1  = 13,
    avcc = 0x200,
    hvcc = 0x201,
    av1c = 0x202,

};
enum
{
    sequence_header = 0,
    avpacket        = 1,
    end_of_sequence = 2,
};

}    // namespace simple_rtmp

#endif
