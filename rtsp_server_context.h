#ifndef SIMPLE_RTMP_RTSP_SERVER_CONTEXT_H
#define SIMPLE_RTMP_RTSP_SERVER_CONTEXT_H

#include <string>
#include <functional>
#include "frame_buffer.h"

namespace simple_rtmp
{
struct rtsp_transport
{
    int transport;    // 1 udp,2 tcp
    int multicast;    // 0 unicast,1-multicast
    uint16_t client_port1;
    uint16_t client_port2;
    int interleaved1;
    int interleaved2;
    uint16_t server_port1;
    uint16_t server_port2;
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
    void input(const simple_rtmp::frame_buffer::ptr& frame);

   private:
    rtsp_server_context_handler handler_;
};

}    // namespace simple_rtmp

#endif
