#ifndef SIMPLE_RTMP_RTMP_FORWARD_SESSION_H
#define SIMPLE_RTMP_RTMP_FORWARD_SESSION_H

#include <queue>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include "execution.h"
#include "channel.h"
#include "frame_buffer.h"
#include "sink.h"
#include "tcp_connection.h"

namespace simple_rtmp
{
class rtmp_forward_session : public std::enable_shared_from_this<rtmp_forward_session>
{
   public:
    explicit rtmp_forward_session(simple_rtmp::executors::executor& ex);
    ~rtmp_forward_session();

   public:
    void start();
    void shutdown();
    boost::asio::ip::tcp::socket& socket();

   private:
    void startup();
    void on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec);
    void on_write(const boost::system::error_code& ec, std::size_t bytes);
    void channel_out(const frame_buffer::ptr& frame, const boost::system::error_code& ec);
    void safe_shutdown();

   private:
    int rtmp_server_send(const simple_rtmp::frame_buffer::ptr& frame);
    int rtmp_server_onplay(const std::string& app, const std::string& stream, double start, double duration, uint8_t reset);
    int rtmp_server_onpause(int, uint32_t);
    int rtmp_server_onseek(uint32_t);
    int rtmp_server_ongetduration(const std::string& app, const std::string& stream, double* duration);

   private:
    std::string stream_id_;
    sink::weak sink_;
    channel::ptr channel_ = nullptr;
    simple_rtmp::executors::executor& ex_;
    std::shared_ptr<tcp_connection> conn_;
    std::vector<frame_buffer::ptr> write_queue_;
    std::vector<frame_buffer::ptr> writing_queue_;
    std::shared_ptr<struct forward_args> args_;
};
}    // namespace simple_rtmp
#endif    //
