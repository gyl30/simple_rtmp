#ifndef SIMPLE_RTMP_HTTP_SESSION_H
#define SIMPLE_RTMP_HTTP_SESSION_H

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <map>
#include "execution.h"
#include "error.h"
#include "log.h"

namespace simple_rtmp
{
class http_session;
using http_session_ptr = std::shared_ptr<http_session>;

using http_request_t = boost::beast::http::request<boost::beast::http::string_body>;
using http_request_ptr = std::shared_ptr<http_request_t>;

using http_response_t = boost::beast::http::response<boost::beast::http::string_body>;
using http_response_ptr = std::shared_ptr<http_response_t>;

using http_request_parser_t = boost::beast::http::request_parser<boost::beast::http::string_body>;
using request_cb_t = std::function<void(http_session_ptr& session, http_request_ptr& req)>;

class http_session : public std::enable_shared_from_this<http_session>
{
   public:
    explicit http_session(simple_rtmp::executors::executor& ex);
    ~http_session() = default;

   public:
    void start();
    void shutdown();
    boost::asio::ip::tcp::socket& socket();
    void write(http_request_ptr& req, http_response_ptr& res);

   private:
    void do_read();
    void on_read(const boost::beast::error_code& ec, std::size_t bytes);
    void safe_do_read();
    void safe_shutdown();
    void on_request();
    void close_socket(boost::asio::ip::tcp::socket& socket);

    void on_write(const http_request_ptr& req, boost::beast::error_code ec, std::size_t bytes);

   public:
    static void register_request_cb(const std::string& name, request_cb_t cb);
    static http_response_ptr create_response(http_request_ptr& req, int code, const std::string& content);

   private:
    static std::map<std::string, request_cb_t> request_cb_;

   private:
    simple_rtmp::executors::executor& ex_;
    boost::asio::ip::tcp::socket socket_{ex_};
    boost::beast::flat_buffer buffer_;
    std::shared_ptr<boost::beast::tcp_stream> stream_;
    boost::optional<http_request_parser_t> parser_;
};
}    // namespace simple_rtmp

#endif
