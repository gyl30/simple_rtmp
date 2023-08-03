#include "http_session.h"

using simple_rtmp::http_session;

http_session::http_session(simple_rtmp::executors::executor& ex) : ex_(ex)
{
}

boost::asio::ip::tcp::socket& http_session::socket()
{
    return socket_;
}

void http_session::start()
{
    std::bind(&http_session::safe_do_read, shared_from_this());
}
void http_session::safe_do_read()
{
    do_read();
}

void http_session::shutdown()
{
    std::bind(&http_session::safe_shutdown, shared_from_this());
}

void http_session::safe_shutdown()
{
    if (!stream_->socket().is_open())
    {
        return;
    }
    boost::system::error_code ignored_ec;
    stream_->socket().close(ignored_ec);
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
    auto req = parser_->release();
    const std::string target = req.target();
    std::string body = req.body();

    LOG_DEBUG("request {} {}", target, body);
}
