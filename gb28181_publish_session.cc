#include "gb28181_publish_session.h"
#include "log.h"

using simple_rtmp::gb28181_publish_session;

gb28181_publish_session::gb28181_publish_session(executors::executor& ex) : ex_(ex), conn_(std::make_shared<tcp_connection>(ex))
{
    LOG_DEBUG("create {}", static_cast<void*>(this));
}
gb28181_publish_session::~gb28181_publish_session()
{
    LOG_DEBUG("destroy {}", static_cast<void*>(this));
}

boost::asio::ip::tcp::socket& gb28181_publish_session::socket()
{
    return conn_->socket();
}

void gb28181_publish_session::start()
{
    conn_->set_read_cb(std::bind(&gb28181_publish_session::on_read, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->set_write_cb(std::bind(&gb28181_publish_session::on_write, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->start();
}

void gb28181_publish_session::on_write(const boost::system::error_code& ec, std::size_t /*bytes*/)
{
    if (ec)
    {
        LOG_ERROR("write failed {} {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
}

void gb28181_publish_session::on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ec)
    {
        shutdown();
        return;
    }
}

void gb28181_publish_session::shutdown()
{
    boost::asio::post(ex_, std::bind(&gb28181_publish_session::safe_shutdown, shared_from_this()));
}

void gb28181_publish_session::safe_shutdown()
{
    if (conn_)
    {
        conn_->shutdown();
        conn_.reset();
    }
}
