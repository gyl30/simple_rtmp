#ifndef SIMPLE_RTMP_RTMP_SERVER_CONTEXT_H
#define SIMPLE_RTMP_RTMP_SERVER_CONTEXT_H

#include <string>
#include <functional>
#include "rtmp-chunk-header.h"
#include "rtmp-netconnection.h"
#include "rtmp-netstream.h"
#include "frame_buffer.h"

namespace simple_rtmp
{

struct rtmp_server_context_handler
{
    std::function<int(const simple_rtmp::frame_buffer::ptr& frame)> send;
    std::function<int(const std::string& app, const std::string& stream, double start, double duration, uint8_t reset)> onplay;
    std::function<int(int pause, uint32_t ms)> onpause;
    std::function<int(uint32_t ms)> onseek;
    std::function<int(const std::string& app, const std::string& stream, const std::string& type)> onpublish;
    std::function<int(const simple_rtmp::frame_buffer::ptr& frame)> onvideo;
    std::function<int(const simple_rtmp::frame_buffer::ptr& frame)> onaudio;
    std::function<int(const simple_rtmp::frame_buffer::ptr& frame)> onscript;
    std::function<int(const std::string& app, const std::string& stream, double* duration)> ongetduration;
};

class rtmp_server_context
{
   public:
    explicit rtmp_server_context(rtmp_server_context_handler handler);

   public:
    int rtmp_server_start(int r, const std::string& msg);
    int rtmp_server_input(const uint8_t* data, size_t bytes);
    int rtmp_server_stop();

   private:
    std::shared_ptr<struct rtmp_server_context_args> args_;
};

}    // namespace simple_rtmp

#endif
