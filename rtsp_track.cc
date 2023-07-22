#include <atomic>
#include <boost/beast/core/detail/base64.hpp>
#include "rtsp_track.h"

namespace simple_rtmp
{

std::string base64_encode(const std::uint8_t* data, std::size_t len)
{
    std::string dest;
    dest.resize(boost::beast::detail::base64::encoded_size(len));
    dest.resize(boost::beast::detail::base64::encode(&dest[0], data, len));
    return dest;
}

std::string base64_encode(const std::string& s)
{
    return base64_encode(reinterpret_cast<std::uint8_t const*>(s.data()), s.size());
}

std::string base64_decode(const std::string& data)
{
    std::string dest;
    dest.resize(boost::beast::detail::base64::decoded_size(data.size()));
    auto const result = boost::beast::detail::base64::decode(&dest[0], data.data(), data.size());
    dest.resize(result.first);
    return dest;
}

uint32_t rtsp_track::audio_ssrc()
{
    static std::atomic<uint32_t> ssrc = 0xff1;
    ssrc += 2;
    return ssrc;
}
uint32_t rtsp_track::video_ssrc()
{
    static std::atomic<uint32_t> ssrc = 0xff2;
    ssrc += 2;
    return ssrc;
}
}    // namespace simple_rtmp
