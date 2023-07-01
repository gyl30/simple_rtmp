#ifndef SIMPLE_RTMP_RTSP_SERVER_CONTEXT_H
#define SIMPLE_RTMP_RTSP_SERVER_CONTEXT_H

#include <string>
#include <functional>
#include "frame_buffer.h"
#include "rtsp_parser.h"

namespace simple_rtmp
{
struct rtsp_transport
{
    int transport = 0;    // 0 udp,1 tcp
    int multicast = 0;    // 0 unicast,1-multicast
    uint16_t client_port1 = 0;
    uint16_t client_port2 = 0;
    int interleaved1 = -1;
    int interleaved2 = -1;
};

struct rtsp_server_context_handler
{
    std::function<int(const std::string& url)> on_options;
    std::function<int(const std::string& url)> on_describe;
    std::function<int(const std::string& url, const std::string& session, rtsp_transport* transport)> on_setup;
    std::function<int(const std::string& url, const std::string& session)> on_play;
    std::function<int(const std::string& url, const std::string& session)> on_teardown;
};

class rtsp_server_context
{
   public:
    explicit rtsp_server_context(rtsp_server_context_handler handler);
    ~rtsp_server_context() = default;

   public:
    // -1 error 0 success
    int input(const simple_rtmp::frame_buffer::ptr& frame);
    int seq() const
    {
        return seq_;
    }

   private:
    int parse_rtsp_message(const simple_rtmp::frame_buffer::ptr& frame);
    int parse_rtcp_message(const simple_rtmp::frame_buffer::ptr& frame);
    void process_request(const simple_rtmp::rtsp_parser& parser);
    void options_request(const simple_rtmp::rtsp_parser& parser);
    void describe_request(const simple_rtmp::rtsp_parser& parser);
    void setup_request(const simple_rtmp::rtsp_parser& parser);
    void play_request(const simple_rtmp::rtsp_parser& parser);
    void teardown_request(const simple_rtmp::rtsp_parser& parser);

   private:
    int seq_ = -1;
    int need_more_data_ = 0;
    simple_rtmp::rtsp_parser parser_;
    rtsp_server_context_handler handler_;
};

}    // namespace simple_rtmp

#endif
