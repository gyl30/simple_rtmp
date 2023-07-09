#ifndef SIMPLE_RTMP_RTSP_TRACK_H
#define SIMPLE_RTMP_RTSP_TRACK_H

#include <string>
#include <memory>
#include <vector>

namespace simple_rtmp
{

const static char kRtspVideoTrackId[] = "track1";
const static char kRtspAudioTrackId[] = "track2";
std::string base64_decode(const std::string& data);
std::string base64_encode(const std::uint8_t* data, std::size_t len);
std::string base64_encode(const std::string& s);

class rtsp_track
{
   public:
    using ptr = std::shared_ptr<rtsp_track>;
    virtual ~rtsp_track() = default;
   public:
    static uint32_t audio_ssrc();
    static uint32_t video_ssrc();
   public:
    virtual std::string sdp() const = 0;
    virtual uint32_t ssrc() const = 0;
    virtual std::string id() const = 0;
};

}    // namespace simple_rtmp

#endif
