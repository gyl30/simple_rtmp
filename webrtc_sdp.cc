#include "webrtc_sdp.h"
#include "sdp-a-webrtc.h"
#include <cstring>
#include <boost/algorithm/string.hpp>

using simple_rtmp::webrtc_sdp;

webrtc_sdp::webrtc_sdp(const std::string& sdp) : sdp_str_(sdp)
{
}
int webrtc_sdp::parse()
{
    if (sdp_ == nullptr)
    {
        sdp_ = sdp_parse(sdp_str_.data(), static_cast<int>(sdp_str_.size()));
    }
    if (sdp_ == nullptr)
    {
        return -1;
    }
    // v=0
    v_.version = sdp_version_get(sdp_);

    // o=- 87905 2 IN IP4 127.0.0.1

    {
        const char* username = nullptr;
        const char* session = nullptr;
        const char* version = nullptr;
        const char* network = nullptr;
        const char* address_type = nullptr;
        const char* address = nullptr;

        int ret = sdp_origin_get(sdp_, &username, &session, &version, &network, &address_type, &address);
        if (ret == 0)
        {
            o_.username = username;
            o_.session_id = session;
            o_.address = address;
            o_.session_version = version;
            o_.nettype = network;
            o_.addrtype = address_type;
        }
    }
    // s=-
    const char* session_name = sdp_session_get_name(sdp_);
    const char* session_info = sdp_session_get_name(sdp_);
    if (session_name != nullptr)
    {
        session_name_ = session_name;
    }
    if (session_info != nullptr)
    {
        session_info_ = session_info;
    }
    // t=0 0
    int time_count = sdp_timing_count(sdp_);
    for (int i = 0; i < time_count; i++)
    {
        const char* start = nullptr;
        const char* stop = nullptr;
        int ret = sdp_timing_get(sdp_, i, &start, &stop);
        if (ret == 0)
        {
            simple_rtmp::webrtc_sdp::time t;
            t.start = atoi(start);
            t.stop = atoi(stop);
            times_.push_back(t);
        }
    }
    // a=group:BUNDLE 0 1
    // a=extmap-allow-mixed
    // a=msid-semantic: WMS
    sdp_attribute_list(
        sdp_,
        nullptr,
        [](void* param, const char* name, const char* value)
        {
            webrtc_sdp* self = (webrtc_sdp*)param;
            if (strncmp("group", name, 6) == 0)
            {
                std::vector<std::string> mids;

                boost::split(mids, value, boost::is_any_of(","), boost::token_compress_on);
                if (mids.empty())
                {
                    return;
                }
                self->group_.type = mids.front();
                mids.erase(mids.begin());
                self->group_.mids = mids;
                return;
            }
            else if (strncmp("msid-semantic", name, 14) == 0)
            {
                self->msid_semantic_.token = value;
            }
        },
        this);
    int media_count = sdp_media_count(sdp_);
    for (int i = 0; i < media_count; i++)
    {
        const auto* type = sdp_media_type(sdp_, i);
        const auto* proto = sdp_media_proto(sdp_, i);
        int format_count = sdp_media_formats(sdp_, i, nullptr, 0);
        int* formats = static_cast<int*>(malloc(format_count));
        sdp_media_formats(sdp_, i, formats, format_count);
        webrtc_sdp::media m;
        m.proto = proto;
        for (int i = 0; i < format_count; i++)
        {
            m.fmts.push_back(formats[i]);
        }
        free(formats);
        int ports[32] = {0};
        int ports_num = sdp_media_port(sdp_, i, ports, 32);
        for (int i = 0; i < ports_num; i++)
        {
            m.ports.push_back(ports[i]);
        }
        medias_.push_back(m);
    }
    if (sdp_origin_get_network(sdp_) == SDP_C_NETWORK_UNKNOWN)
    {
        return -1;
    }
    if (sdp_origin_get_addrtype(sdp_) == SDP_C_ADDRESS_UNKNOWN)
    {
        return -1;
    }
    if (sdp_session_get_name(sdp_) == nullptr)
    {
        return -1;
    }
    if (sdp_session_get_information(sdp_) == nullptr)
    {
        return -1;
    }
    if (sdp_version_get(sdp_) != 0)
    {
        return -1;
    }
    return 0;
}

int webrtc_sdp::fingerprint_algorithm(std::string& algorithm, std::string& fingerprint)
{
    const char* fingerprint_str = sdp_media_attribute_find(sdp_, 0, "fingerprint");
    if (fingerprint_str == nullptr)
    {
        return -1;
    }
    int fingerprint_len = strlen(fingerprint_str);
    char hash[64] = {0};
    char fingerprint_buff[1024] = {0};
    const int ret = sdp_a_fingerprint(fingerprint_str, fingerprint_len, hash, fingerprint_buff);
    if (ret != 0)
    {
        return -1;
    }
    algorithm = hash;
    fingerprint = fingerprint_buff;
    return 0;
}
