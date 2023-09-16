#ifndef SIMPLE_RTMP_WEBRTC_SDP_H
#define SIMPLE_RTMP_WEBRTC_SDP_H

#include <map>
#include <string>
#include <vector>
#include <set>
#include "sdp.h"

namespace simple_rtmp
{
class webrtc_sdp
{
    struct version
    {
        std::string key = "v";
        int version;
    };
    struct connection
    {
        std::string nettype{"IN"};
        std::string addrtype{"IP4"};
        std::string address{"0.0.0.0"};
    };
    struct time
    {
        std::string key = "t";
        uint64_t start = 0;
        uint64_t stop = 0;
    };
    struct bandwidth
    {
        std::string key = "b";
        std::string bwtype{"AS"};
        uint32_t bandwidth{0};
    };

    struct origin
    {
        std::string key = "o";
        std::string username{"-"};
        std::string session_id;
        std::string session_version;
        std::string nettype{"IN"};
        std::string addrtype{"IP4"};
        std::string address{"0.0.0.0"};
    };
    struct attribute
    {
        std::string key = "a";
        std::string attr;
    };
    struct attribute_group
    {
        std::string key = "group";
        std::string type{"BUNDLE"};
        std::vector<std::string> mids;
    };
    struct attribute_msid_semantic
    {
        std::string key = "msid-semantic";
        std::string msid{"WMS"};
        std::string token;
    };
    struct attribute_rtcp
    {
        uint16_t port[2] = {0};
        std::string key = "rtcp";
        std::string nettype;
        std::string addrtype;
        std::string address;
        std::string source;
    };
    struct attribute_ice_ufrag
    {
        std::string key = "ice-ufrag";
        std::string attr;
    };

    struct attribute_ice_pwd
    {
        std::string key = "ice-pwd";
        std::string attr;
    };

    struct attribute_ice_option
    {
        std::string key = "ice-options";
        bool trickle = false;
        bool renomination = false;
    };
    struct attribute_mid
    {
        std::string key = "mid";
        std::string mid;
    };

    struct attribute_extmap
    {
        uint8_t id;
        std::string ext;
        std::string direction;
        std::string key = "extmap";
    };
    struct attribute_rtpmap
    {
        uint8_t pt;
        std::string key = "rtpmap";
        std::string codec;
        uint32_t sample_rate;
        uint32_t channel = 0;
    };
    struct attribute_rtcp_fb
    {
        uint8_t pt;
        std::string key = "rtcp-fb";
        std::string rtcp_type;
    };
    struct attribute_fmtp
    {
        uint8_t pt;
        std::string key = "fmtp";
        std::map<std::string, std::string> fmtp;
    };
    struct attribute_ssrc
    {
        uint32_t ssrc;
        std::string key = "ssrc";
        std::string attribute;
        std::string attribute_value;
    };
    struct attribute_ssrc_group
    {
        std::string key = "ssrc-group";
        std::string type{"FID"};
        std::vector<uint32_t> ssrcs;
    };
    struct attribute_sctpmap
    {
        uint16_t port = 0;
        uint32_t streams = 0;
        std::string key = "sctpmap";
        std::string subtypes;
    };
    struct attribute_candidate
    {
        std::string key = "candidate";
        std::string foundation;
        uint32_t component;
        std::string transport{"udp"};
        uint32_t priority;
        std::string address;
        uint16_t port;
        std::string type;
        std::vector<std::pair<std::string, std::string> > arr;
    };
    struct attribute_msid
    {
        std::string key = "msid";
        std::string value;
    };
    struct attribute_extmap_allow_mixed
    {
        std::string key = "extmap-allow-mixed";
        std::string value;
    };
    struct attribute_simulcast
    {
        std::string key = "simulcast";
        std::string direction;
        std::vector<std::string> rids;
    };
    struct attribute_rid
    {
        std::string direction;
        std::string rid;
        std::string key = "rid";
    };
    /////////////////////////////////////////////////////////////
    struct rtc_codec_plan
    {
        uint8_t pt;
        std::string codec;
        uint32_t sample_rate;
        uint32_t channel = 0;
        std::set<std::string> rtcp_fb;
        std::map<std::string, std::string> fmtp;
    };
    struct attribute_fingerprint
    {
        std::string key = "fingerprint";
        std::string algorithm;
        std::string fingerprint;
    };
    struct rtc_media
    {
        std::string mid;
        std::string type;    // audio video
        std::string ice_ufrag;
        std::string ice_pwd;
        std::string ice_options;
        attribute_fingerprint fingerprint;
        std::string setup;
        std::vector<int> fmts;
        std::vector<uint16_t> ports;
        webrtc_sdp::connection c;
        webrtc_sdp::attribute_rtcp attr_rtcp;
        webrtc_sdp::bandwidth bandwidth;
        std::string proto;
        std::string direction;    // sendonly recvonly ...
    };

   public:
    explicit webrtc_sdp(std::string sdp);
    ~webrtc_sdp() = default;

   public:
    int parse();
    int fingerprint_algorithm(std::string& algorithm, std::string& fingerprint);

   private:
    int parse_attribute();
    int parse_media_attribute(int media_index);

   private:
    version v_;
    origin o_;
    std::string session_name_;
    std::string session_info_;
    std::vector<time> times_;
    connection c_;
    attribute_group group_;
    attribute_msid_semantic msid_semantic_;
    std::vector<rtc_media> medias_;
    sdp_t* sdp_ = nullptr;
    std::string sdp_str_;
};
}    // namespace simple_rtmp

#endif
