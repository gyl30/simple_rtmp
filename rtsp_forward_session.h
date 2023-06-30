#ifndef SIMPLE_RTMP_RTSP_FORWARD_SESSION_H
#define SIMPLE_RTMP_RTSP_FORWARD_SESSION_H

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

class rtsp_forward_session : public std::enable_shared_from_this<rtsp_forward_session>
{
   public:
    explicit rtsp_forward_session(simple_rtmp::executors::executor& ex);
    ~rtsp_forward_session();

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
    std::string stream_id_;
    sink::weak sink_;
    channel::ptr channel_ = nullptr;
    simple_rtmp::executors::executor& ex_;
    std::shared_ptr<tcp_connection> conn_;
    std::vector<frame_buffer::ptr> write_queue_;
    std::vector<frame_buffer::ptr> writing_queue_;
    std::shared_ptr<struct rtsp_forward_args> args_;
};

}    // namespace simple_rtmp

#endif