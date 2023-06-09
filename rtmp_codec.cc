#include <string>
#include "rtmp_codec.h"

std::string simple_rtmp::rtmp_tag_to_str(int tag)
{
    switch (tag)
    {
        case simple_rtmp::rtmp_tag::script:
            return "script";
        case simple_rtmp::rtmp_tag::video:
            return "video";
        case simple_rtmp::rtmp_tag::audio:
            return "audio";
    }
    return "unknown";
}
std::string simple_rtmp::rtmp_codec_to_str(int codec)
{
    switch (codec)
    {
        case simple_rtmp::rtmp_codec::h264:
            return "h264";
        case simple_rtmp::rtmp_codec::h265:
            return "h265";
        case simple_rtmp::rtmp_codec::aac:
            return "aac";
        case simple_rtmp::rtmp_codec::opus:
            return "opus";
    }
    return "unknown";
}
