#ifndef SIMPLE_RTMP_HTTP_SESSION_H
#define SIMPLE_RTMP_HTTP_SESSION_H

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include "execution.h"
#include "error.h"
#include "log.h"

namespace simple_rtmp
{
class http_session : public std::enable_shared_from_this<http_session>
{
   public:
    http_session(simple_rtmp::executors::executor& ex);
    ~http_session() = default;

   public:
    void start();
    void shutdown();
    boost::asio::ip::tcp::socket& socket();

   private:
    void do_read();
    void on_read(const boost::system::error_code& ec, std::size_t bytes);

   private:
    simple_rtmp::executors::executor& ex_;
    boost::asio::ip::tcp::socket socket_{ex_};
    boost::beast::flat_buffer buffer_;
    std::shared_ptr<boost::beast::tcp_stream> stream_;
};
}    // namespace simple_rtmp

#endif
