#include "log.h"
#include "socket.h"
#include "flv_session.h"

using simple_rtmp::flv_session;

flv_session::flv_session(simple_rtmp::executors::executor& ex, const std::shared_ptr<boost::beast::tcp_stream>& stream) : ex_(ex), stream_(stream)
{
}

void flv_session::start()
{
    //
    ex_.post(std::bind(&flv_session::safe_do_read, shared_from_this()));
}
void flv_session::shutdown()
{
    //
}
void flv_session::do_read()
{
    //
}
void flv_session::on_read(const boost::beast::error_code& ec, std::size_t bytes)
{
    //
}
void flv_session::safe_do_read()
{
    //
    auto& socket = stream_->socket();
    std::string local_addr = get_socket_local_address(socket);
    std::string remote_addr = get_socket_remote_address(socket);

    LOG_DEBUG("do read {} <--> {}", local_addr, remote_addr);

    do_read();
}
void flv_session::safe_shutdown()
{
    //
}
void flv_session::close_socket(boost::asio::ip::tcp::socket& socket)
{
    //
}
void flv_session::on_write(const http_request_ptr& req, boost::beast::error_code ec, std::size_t bytes)
{
    //
}
