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
using http_request_t = boost::beast::http::request<boost::beast::http::string_body>;
using http_request_parser_t = boost::beast::http::request_parser<boost::beast::http::string_body>;

class http_session : public std::enable_shared_from_this<http_session>
{
   public:
    explicit http_session(simple_rtmp::executors::executor& ex);
    ~http_session() = default;

   public:
    void start();
    void shutdown();
    boost::asio::ip::tcp::socket& socket();

   private:
    void do_read();
    void on_read(const boost::beast::error_code& ec, std::size_t bytes);
    void safe_do_read();
    void safe_shutdown();
    void on_request();

   private:
    simple_rtmp::executors::executor& ex_;
    boost::asio::ip::tcp::socket socket_{ex_};
    boost::beast::flat_buffer buffer_;
    std::shared_ptr<boost::beast::tcp_stream> stream_;
    boost::optional<http_request_parser_t> parser_;
};
}    // namespace simple_rtmp

#endif
