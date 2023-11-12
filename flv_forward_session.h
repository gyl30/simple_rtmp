#ifndef SIMPLE_RTMP_FLV_FORWARD_SESSION_H
#define SIMPLE_RTMP_FLV_FORWARD_SESSION_H

#include <memory>
#include <string>
#include "execution.h"
#include "channel.h"
#include "frame_buffer.h"
#include "sink.h"
#include "tcp_connection.h"

namespace simple_rtmp
{
class flv_forward_session : public std::enable_shared_from_this<flv_forward_session>
{
   public:
    flv_forward_session(std::string target, simple_rtmp::executors::executor& ex, boost::asio::ip::tcp::socket socket);
    ~flv_forward_session() = default;

   public:
    void start();
    void shutdown();
    boost::asio::ip::tcp::socket& socket();
    void write(const frame_buffer::ptr& frame);

   private:
    void safe_shutdown();
    void channel_out(const frame_buffer::ptr& frame, const boost::system::error_code& ec);

    void on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec);
    void on_write(const boost::system::error_code& ec, std::size_t bytes);

   private:
    bool first_frame_ = false;
    std::string target_;
    sink::weak sink_;
    channel::ptr channel_ = nullptr;
    simple_rtmp::executors::executor& ex_;
    std::shared_ptr<tcp_connection> conn_;
};
}    // namespace simple_rtmp

#endif
