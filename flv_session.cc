#include "flv_session.h"

using simple_rtmp::flv_session;

flv_session::flv_session(simple_rtmp::executors::executor& ex, boost::asio::ip::tcp::socket socket) : ex_(ex), conn_(std::make_shared<tcp_connection>(ex_, std::move(socket)))
{
}

void flv_session::start()
{
    conn_->set_read_cb(std::bind(&flv_session::on_read, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->set_write_cb(std::bind(&flv_session::on_write, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->start();
}

void flv_session::shutdown()
{
    //
    auto self = shared_from_this();

    ex_.post([this, self]() { safe_shutdown(); });
}

void flv_session::safe_shutdown()
{
    //

    conn_->shutdown();
    conn_.reset();
}

void flv_session::on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec)
{
    //
}

void flv_session::write_frame(const simple_rtmp::frame_buffer::ptr& frame)
{
    conn_->write_frame(frame);
}

void flv_session::on_write(const boost::system::error_code& ec, std::size_t bytes)
{
    //
}
