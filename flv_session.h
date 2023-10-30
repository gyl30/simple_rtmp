#ifndef SIMPLE_RTMP_FLV_SESSION_H
#define SIMPLE_RTMP_FLV_SESSION_H

#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "http_session.h"

namespace simple_rtmp
{
class flv_session : public std::enable_shared_from_this<flv_session>
{
   public:
    explicit flv_session(simple_rtmp::executors::executor& ex, const std::shared_ptr<boost::beast::tcp_stream>& stream);
    ~flv_session() = default;

   public:
    void start();
    void shutdown();

   private:
    void do_read();
    void on_read(const boost::beast::error_code& ec, std::size_t bytes);
    void safe_do_read();
    void safe_shutdown();
    void close_socket(boost::asio::ip::tcp::socket& socket);
    void on_write(const http_request_ptr& req, boost::beast::error_code ec, std::size_t bytes);

   private:
    simple_rtmp::executors::executor& ex_;
    std::shared_ptr<boost::beast::tcp_stream> stream_;
};

}    // namespace simple_rtmp

#endif
