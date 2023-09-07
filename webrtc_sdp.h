#ifndef SIMPLE_RTMP_WEBRTC_SDP_H
#define SIMPLE_RTMP_WEBRTC_SDP_H

#include <string>
#include <vector>
#include "sdp.h"

namespace simple_rtmp
{
class webrtc_sdp
{
    struct version
    {
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
        uint64_t start = 0;
        uint64_t stop = 0;
    };
    struct bandwidth
    {
        std::string bwtype{"AS"};
        uint32_t bandwidth{0};
    };
    struct media
    {
        uint16_t port;
        std::string proto;
        std::vector<std::string> fmts;
    };

    struct origin
    {
        std::string username{"-"};
        std::string session_id;
        std::string session_version;
        std::string nettype{"IN"};
        std::string addrtype{"IP4"};
        std::string address{"0.0.0.0"};
    };
    struct attribute
    {
        std::string attr;
    };
    struct attr_group
    {
        std::string key;
        std::string type{"BUNDLE"};
        std::vector<std::string> mids;
    };
    struct attribute_msid_semantic
    {
        std::string key;
        std::string msid{"WMS"};
        std::string token;
    };
    struct attribute_rtcp
    {
        uint16_t port{0};
        std::string key;
        std::string nettype{"IN"};
        std::string addrtype{"IP4"};
        std::string address{"0.0.0.0"};
    };
    struct attribute_ice_ufrag
    {
        std::string key;
        std::string attr;
    };

    struct attribute_ice_pwd
    {
        std::string key;
        std::string attr;
    };

    struct attribute_ice_option
    {
        std::string key;
        bool trickle = false;
        bool renomination = false;
    };
    struct attribute_fingerprint
    {
        std::string algorithm;
        std::string hash;
    };
    struct attribute_setup
    {
        std::string key;
        std::string setup;
    };
    struct attribute_mid
    {
        std::string key;
        std::string mid;
    };

    struct attribute_extmap
    {
        uint8_t id;
        std::string ext;
        std::string direction;
        std::string key;
    };
    struct attribute_rtpmap
    {
        uint8_t pt;
        std::string key;
        std::string codec;
        uint32_t sample_rate;
        uint32_t channel = 0;
    };
    struct attribute_rtcp_fb
    {
        uint8_t pt;
        std::string key;
        std::string rtcp_type;
    };
    struct attribute_fmtp
    {
        uint8_t pt;
        std::string key;
        std::map<std::string, std::string> fmtp;
    };
    struct attribute_ssrc
    {
        uint32_t ssrc;
        std::string key;
        std::string attribute;
        std::string attribute_value;
    };
    struct attribute_ssrc_group
    {
        std::string key;
        std::string type{"FID"};
        std::vector<uint32_t> ssrcs;
    };
    struct attribute_sctpmap
    {
        uint16_t port = 0;
        uint32_t streams = 0;
        std::string key;
        std::string subtypes;
    };
    struct attribute_candidate
    {
        std::string key;
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
        std::string key;
        std::string value;
    };
    struct attribute_extmap_allow_mixed
    {
        std::string key;
        std::string value;
    };
    struct attribute_simulcast
    {
        std::string key;
        std::string direction;
        std::vector<std::string> rids;
    };
    struct attribute_rid
    {
        std::string direction;
        std::string rid;
    };

   public:
    explicit webrtc_sdp(const std::string& sdp);
    ~webrtc_sdp() = default;

   public:
    int parse();
    int fingerprint_algorithm(std::string& algorithm, std::string& fingerprint);

   private:
    sdp_t* sdp_ = nullptr;
    std::string sdp_str_;
};
}    // namespace simple_rtmp

#endif
