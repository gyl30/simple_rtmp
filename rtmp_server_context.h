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
    static int rtmp_server_send_onstatus(rtmp_server_context* ctx, double transaction, int r, const char* success, const char* fail, const char* description);
    static int rtmp_server_send_handshake(rtmp_server_context* ctx);
    static int rtmp_server_send_set_chunk_size(rtmp_server_context* ctx);
    static int rtmp_server_send_acknowledgement(rtmp_server_context* ctx, size_t size);
    static int rtmp_server_send_server_bandwidth(rtmp_server_context* ctx);
    static int rtmp_server_send_client_bandwidth(rtmp_server_context* ctx);
    static int rtmp_server_send_stream_is_record(rtmp_server_context* ctx);
    static int rtmp_server_send_stream_begin(rtmp_server_context* ctx);
    static int rtmp_server_rtmp_sample_access(rtmp_server_context* ctx);
    static void rtmp_server_onabort(void* param, uint32_t chunk_stream_id);
    static int rtmp_server_onconnect(void* param, int r, double transaction, const struct rtmp_connect_t* connect);
    static int rtmp_server_onaudio(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp);
    static int rtmp_server_onvideo(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp);
    static int rtmp_server_onscript(void* param, const uint8_t* data, size_t bytes, uint32_t timestamp);
    static int rtmp_server_oncreate_stream(void* param, int r, double transaction);
    static int rtmp_server_ondelete_stream(void* param, int r, double transaction, double stream_id);
    static int rtmp_server_onget_stream_length(void* param, int r, double transaction, const char* stream_name);
    static int rtmp_server_onpublish(void* param, int r, double transaction, const char* stream_name, const char* stream_type);
    static int rtmp_server_onplay(void* param, int r, double transaction, const char* stream_name, double start, double duration, uint8_t reset);
    static int rtmp_server_onpause(void* param, int r, double transaction, uint8_t pause, double milliSeconds);
    static int rtmp_server_onseek(void* param, int r, double transaction, double milliSeconds);
    static int rtmp_server_onreceive_audio(void* param, int r, double transaction, uint8_t audio);
    static int rtmp_server_onreceive_video(void* param, int r, double transaction, uint8_t video);
    static int rtmp_server_send(void* param, const uint8_t* header, uint32_t headerBytes, const uint8_t* payload, uint32_t payloadBytes);

   private:
    std::shared_ptr<struct rtmp_server_context_args> args_;
    rtmp_server_context_handler handler_;
};

}    // namespace simple_rtmp

#endif
