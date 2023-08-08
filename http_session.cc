#include <utility>
#include "http_session.h"
#include "socket.h"

using simple_rtmp::http_session;

std::map<std::string, simple_rtmp::request_cb_t> http_session::request_cb_;

http_session::http_session(simple_rtmp::executors::executor& ex) : ex_(ex)
{
}

boost::asio::ip::tcp::socket& http_session::socket()
{
    return socket_;
}

void http_session::start()
{
    ex_.post(std::bind(&http_session::safe_do_read, shared_from_this()));
}

void http_session::safe_do_read()
{
    std::string local_addr = get_socket_local_address(socket_);
    std::string remote_addr = get_socket_remote_address(socket_);
    stream_ = std::make_shared<boost::beast::tcp_stream>(std::move(socket_));

    LOG_DEBUG("do read {} <--> {}", local_addr, remote_addr);

    do_read();
}

void http_session::shutdown()
{
    ex_.post(std::bind(&http_session::safe_shutdown, shared_from_this()));
}

void http_session::safe_shutdown()
{
    if (stream_)
    {
        close_socket(stream_->socket());
    }
    close_socket(socket_);
}

void http_session::close_socket(boost::asio::ip::tcp::socket& socket)
{
    if (!socket.is_open())
    {
        return;
    }

    boost::system::error_code ignored_ec;
    std::string local_addr = get_socket_local_address(socket);
    std::string remote_addr = get_socket_remote_address(socket);
    LOG_WARN("shutdown {} <--> {}", local_addr, remote_addr);
    socket.close(ignored_ec);
}

void http_session::do_read()
{
    parser_.emplace();

    constexpr auto kMaxBodyLimit = 1024;
    parser_->body_limit(kMaxBodyLimit);
    auto fn = boost::beast::bind_front_handler(&http_session::on_read, shared_from_this());
    boost::beast::http::async_read(*stream_, buffer_, parser_->get(), std::move(fn));
}

void http_session::on_read(const boost::beast::error_code& ec, std::size_t /*unused*/)
{
    if (ec)
    {
        LOG_ERROR("read failed {}", ec.message());
        shutdown();
        return;
    }
    on_request();
}

void http_session::on_request()
{
    if (!parser_)
    {
        shutdown();
        return;
    }
    auto req = std::make_shared<http_request_t>(std::move(parser_->release()));
    const std::string target = req->target();
    std::string body = req->body();

    LOG_DEBUG("request {} {}", target, body);

    auto it = request_cb_.find(target);
    if (it == request_cb_.end())
    {
        shutdown();
        return;
    }
    http_session_ptr session = shared_from_this();

    it->second(session, req);
}

void http_session::write(http_request_ptr& req, http_response_ptr& res)
{
    auto self = shared_from_this();
    boost::beast::http::async_write(*stream_, *res, [self, this, req, res](boost::beast::error_code ec, std::size_t bytes) { on_write(req, ec, bytes); });
}

simple_rtmp::http_response_ptr http_session::create_response(simple_rtmp::http_request_ptr& req, int code, const std::string& content)
{
    auto status = boost::beast::http::int_to_status(code);
    auto rsp = std::make_shared<boost::beast::http::response<boost::beast::http::string_body>>(status, req->version());
    rsp->body().assign(content);
    rsp->keep_alive(req->keep_alive());
    rsp->content_length(content.size());
    rsp->set(boost::beast::http::field::server, "simple/rtmp");
    return rsp;
}

void http_session::on_write(const http_request_ptr& req, boost::beast::error_code ec, std::size_t bytes)
{
    if (!req->keep_alive())
    {
        shutdown();
        return;
    }
    LOG_DEBUG("keep alive");
    do_read();
}

/////////////////////////////////////////////////////////////////////////////
///

void http_session::register_request_cb(const std::string& name, request_cb_t cb)
{
    request_cb_[name] = std::move(cb);
}