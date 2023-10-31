#ifndef SIMPLE_RTMP_FLV_SESSION_H
#define SIMPLE_RTMP_FLV_SESSION_H

#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "http_session.h"
#include "tcp_connection.h"

namespace simple_rtmp
{
class flv_session : public std::enable_shared_from_this<flv_session>
{
   public:
    flv_session(simple_rtmp::executors::executor& ex, boost::asio::ip::tcp::socket socket);
    ~flv_session() = default;

   public:
    void start();
    void shutdown();

    void write_frame(const simple_rtmp::frame_buffer::ptr& frame);

   private:
    void on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec);
    void on_write(const boost::system::error_code& ec, std::size_t bytes);
    void safe_shutdown();

   private:
    simple_rtmp::executors::executor& ex_;
    std::shared_ptr<tcp_connection> conn_;
};

}    // namespace simple_rtmp

#endif
