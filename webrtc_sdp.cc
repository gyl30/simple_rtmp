#include "webrtc_sdp.h"
#include <cstring>
#include <boost/algorithm/string.hpp>
#include <utility>
#include <cstdlib>
extern "C"
{
#include "sdp-a-webrtc.h"
#include "sdp-options.h"
#include "sdp-a-rtpmap.h"
#include "sdp-a-fmtp.h"
}

using simple_rtmp::webrtc_sdp;

webrtc_sdp::webrtc_sdp(std::string sdp) : sdp_str_(std::move(sdp))
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
    if (sdp_version_get(sdp_) != 0)
    {
        return -1;
    }

    parse_attribute();

    int media_count = sdp_media_count(sdp_);
    for (int i = 0; i < media_count; i++)
    {
        parse_media_attribute(i);
    }
    return 0;
}

int webrtc_sdp::parse_media_attribute(int media_index)
{
    const auto* type = sdp_media_type(sdp_, media_index);
    const auto* proto = sdp_media_proto(sdp_, media_index);
    int format_count = sdp_media_formats(sdp_, media_index, nullptr, 0);
    int* formats = static_cast<int*>(malloc(format_count));
    if (sdp_media_formats(sdp_, media_index, formats, format_count) == -1)
    {
        return -1;
    }
    webrtc_sdp::rtc_media m;
    m.proto = proto;
    m.type = type;

    for (int i = 0; i < format_count; i++)
    {
        m.fmts.push_back(formats[i]);
    }
    free(formats);
    int ports[32] = {0};
    int ports_num = sdp_media_port(sdp_, media_index, ports, 32);
    for (int i = 0; i < ports_num; i++)
    {
        m.ports.push_back(ports[i]);
    }
    const char* network = nullptr;
    const char* address_type = nullptr;
    const char* address = nullptr;

    int ret = sdp_media_get_connection(sdp_, media_index, &network, &address_type, &address);
    if (ret == 0)
    {
        m.c.address = address;
        m.c.addrtype = address_type;
        m.c.nettype = network;
    }
    using attribute_kv = std::vector<std::pair<std::string, std::string>>;
    attribute_kv kv;
    sdp_media_attribute_list(
        sdp_,
        media_index,
        nullptr,
        [](void* param, const char* name, const char* value)
        {
            auto* kv = static_cast<attribute_kv*>(param);
            if (value != nullptr)
            {
                auto pair = std::make_pair(name, value);
                kv->emplace_back(pair);
            }
        },
        &kv);

    for (const auto& pair : kv)
    {
        const std::string& name = pair.first;
        const std::string& value = pair.second;
        if (name == "rtcp")
        {
            sdp_address_t addr;
            if (sdp_a_rtcp(value.data(), static_cast<int>(value.size()), &addr) == 0)
            {
                m.attr_rtcp.nettype = addr.network;
                m.attr_rtcp.addrtype = addr.addrtype;
                m.attr_rtcp.address = addr.address;
                m.attr_rtcp.source = addr.source;
                m.attr_rtcp.port[0] = addr.port[0];
                m.attr_rtcp.port[1] = addr.port[1];
            }
            continue;
        }

        if (name == "ice-ufrag")
        {
            m.ice_ufrag = value;
            continue;
        }
        if (name == "ice-pwd")
        {
            m.ice_pwd = value;
            continue;
        }
        if (name == "ice-options")
        {
            m.ice_options = value;
            continue;
        }
        if (name == "fingerprint")
        {
            char hash[16];
            char fingerprint[128];

            int ret = sdp_a_fingerprint(name.data(), static_cast<int>(name.size()), hash, fingerprint);
            if (ret == 0)
            {
                m.fingerprint.algorithm = hash;
                m.fingerprint.fingerprint = fingerprint;
            }
            continue;
        }
        if (name == "setup")
        {
            m.setup = value;
            continue;
        }
        if (name == "mid")
        {
            char tag[256] = {0};
            if (sdp_a_mid(value.data(), static_cast<int>(value.size()), tag) != -1)
            {
                m.mid = tag;
            }
        }
        if (name == "extmap")
        {
            char url[128];
            int ext = -1;
            int direction = -1;
            if (sdp_a_extmap(value.data(), static_cast<int>(value.size()), &ext, &direction, url) != 0)
            {
                continue;
            }
            attribute_extmap extmap;
            extmap.ext = ext;
            extmap.direction = direction;
            extmap.url = url;
            m.extmaps.push_back(extmap);
            continue;
        }

        if (name == "msid")
        {
            char msid[65];
            char appdata[65];

            if (sdp_a_msid(value.data(), static_cast<int>(value.size()), msid, appdata) != 0)
            {
                continue;
            }
            m.msid.msid = msid;
            m.msid.appdata = appdata;
            continue;
        }
        if (name == "rtpmap")
        {
            int payload = 0;
            int rate = 0;
            char encoding[32] = {0};
            char parameters[64] = {0};
            if (sdp_a_rtpmap(value.data(), &payload, encoding, &rate, parameters) != 0)
            {
                continue;
            }
            attribute_rtpmap rtpmap;
            rtpmap.pt = payload;
            rtpmap.sample_rate = rate;
            rtpmap.codec = encoding;
            if (strlen(parameters) != 0)
            {
                rtpmap.channel = atoi(parameters);
            }
            m.rtpmaps_.push_back(rtpmap);
            continue;
        }
        if (name == "rtcp-fb")
        {
            struct sdp_rtcp_fb_t fb;
            memset(&fb, 0, sizeof(fb));
            if (sdp_a_rtcp_fb(value.data(), static_cast<int>(value.size()), &fb) != 0)
            {
                continue;
            }
            attribute_rtcp_fb rtcp_fb;
            rtcp_fb.feedback = fb.feedback;
            rtcp_fb.fmt = fb.fmt;
            rtcp_fb.param = fb.param;
            rtcp_fb.trr_int = fb.trr_int;
            m.rtcp_fbs_.push_back(rtcp_fb);
            continue;
        }
        if (name == "rtcp-mux")
        {
            m.rtcp_mux_ = name;
            continue;
        }
        if (name == "ice-lite")
        {
            m.ice_lite_ = name;
            continue;
        }
        if (name == "fmtp")
        {
            int fmt = 0;
            char* params = nullptr;
            if (sdp_a_fmtp(value.data(), static_cast<int>(value.size()), &fmt, &params) != 0)
            {
                continue;
            }
            std::vector<std::string> kvs;
            boost::split(kvs, params, boost::is_any_of(";"));
            for (const auto& kv_str : kvs)
            {
                std::vector<std::string> kv;
                boost::split(kv, kv_str, boost::is_any_of("="));
                if (kv.size() == 2)
                {
                    m.fmtp_.params.emplace(kv.front(), kv.back());
                }
            }
            free(params);

            m.fmtp_.fmt = fmt;
        }
        if (name == "ssrc")
        {
            uint32_t ssrc = 0;
            char attribute[64] = {0};
            char attribute_value[128] = {0};
            if (sdp_a_ssrc(value.data(), static_cast<int>(value.size()), &ssrc, attribute, attribute_value) != 0)
            {
                continue;
            }
            attribute_ssrc attr_ssrc;
            attr_ssrc.ssrc = ssrc;
            attr_ssrc.attribute = attribute;
            attr_ssrc.attribute_value = attribute_value;
            m.attr_ssrc_.push_back(attr_ssrc);
            continue;
        }
        if (name == "rid")
        {
            sdp_rid_t rid;
            if (sdp_a_rid(value.data(), static_cast<int>(value.size()), &rid) != 0)
            {
                continue;
            }
            m.rid_.direction = rid.direction;
            m.rid_.rid = rid.rid;
            continue;
        }
        if (name == "ssrc-group")
        {
            sdp_ssrc_group_t ssrc_group;
            if (sdp_a_ssrc_group(value.data(), static_cast<int>(value.size()), &ssrc_group) != 0)
            {
                continue;
            }
            m.ssrc_group_.type = ssrc_group.key;
            for (int i = 0; i < ssrc_group.count; i++)
            {
                m.ssrc_group_.ssrcs.emplace_back(ssrc_group.values[i]);
            }
            free(ssrc_group.values);
            continue;
        }
    }
    medias_.push_back(m);
    return 0;
}

int webrtc_sdp::parse_attribute()
{
    // v=0
    v_.version = sdp_version_get(sdp_);
    // o=
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
    // i=
    const char* session_info = sdp_session_get_information(sdp_);
    if (session_info != nullptr)
    {
        session_info_ = session_info;
    }

    // s=-
    const char* session_name = sdp_session_get_name(sdp_);
    if (session_name != nullptr)
    {
        session_name_ = session_name;
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
            auto* self = static_cast<webrtc_sdp*>(param);
            if (strncmp(name, "group", 5) == 0)
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
            if (strncmp(name, "msid-semantic", 13) == 0)
            {
                self->msid_semantic_.token = value;
            }
        },
        this);
    return 0;
}

webrtc_sdp::ptr webrtc_sdp::create_answer()
{
    //
    return nullptr;
}
int webrtc_sdp::fingerprint_algorithm(std::string& algorithm, std::string& fingerprint)
{
    const char* fingerprint_str = sdp_media_attribute_find(sdp_, 0, "fingerprint");
    if (fingerprint_str == nullptr)
    {
        return -1;
    }
    auto fingerprint_len = strlen(fingerprint_str);
    char hash[64] = {0};
    char fingerprint_buff[1024] = {0};
    const int ret = sdp_a_fingerprint(fingerprint_str, static_cast<int>(fingerprint_len), hash, fingerprint_buff);
    if (ret != 0)
    {
        return -1;
    }
    algorithm = hash;
    fingerprint = fingerprint_buff;
    return 0;
}
