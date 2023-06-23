#include "rtsp_forward_session.h"
#include "socket.h"
#include "log.h"
#include "sink.h"
#include "frame_buffer.h"

using simple_rtmp::rtsp_forward_session;
using namespace std::placeholders;

struct simple_rtmp::rtsp_forward_args
{
    std::string app;
    std::string stream;
};

rtsp_forward_session::rtsp_forward_session(simple_rtmp::executors::executor& ex) : ex_(ex), conn_(std::make_shared<tcp_connection>(ex_))
{
    LOG_DEBUG("create {}", static_cast<void*>(this));
};

rtsp_forward_session::~rtsp_forward_session()
{
    if (args_)
    {
        args_.reset();
    }
    LOG_DEBUG("destroy {}", static_cast<void*>(this));
}

boost::asio::ip::tcp::socket& rtsp_forward_session::socket()
{
    return conn_->socket();
}
void rtsp_forward_session::start()
{
    args_    = std::make_shared<simple_rtmp::rtsp_forward_args>();
    channel_ = std::make_shared<simple_rtmp::channel>();
    channel_->set_output(std::bind(&rtsp_forward_session::channel_out, shared_from_this(), _1, _2));
    conn_->set_read_cb(std::bind(&rtsp_forward_session::on_read, shared_from_this(), _1, _2));
    conn_->set_write_cb(std::bind(&rtsp_forward_session::on_write, shared_from_this(), _1, _2));
    conn_->start();
}

void rtsp_forward_session::channel_out(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        LOG_ERROR("channel out {} failed {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
}

void rtsp_forward_session::on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec)
{
    if (ec && ec == boost::asio::error::bad_descriptor)
    {
        return;
    }

    if (ec)
    {
        LOG_ERROR("read failed {} {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
}

void rtsp_forward_session::on_write(const boost::system::error_code& ec, std::size_t /*bytes*/)
{
    if (ec && ec == boost::asio::error::bad_descriptor)
    {
        return;
    }

    if (ec)
    {
        LOG_ERROR("read failed {} {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
}

void rtsp_forward_session::shutdown()
{
    ex_.post(std::bind(&rtsp_forward_session::safe_shutdown, shared_from_this()));
}

void rtsp_forward_session::safe_shutdown()
{
    LOG_DEBUG("shutdown {}", static_cast<void*>(this));
    auto s = sink_.lock();
    if (s)
    {
        s->del_channel(channel_);
    }
    if (channel_)
    {
        channel_.reset();
    }

    if (conn_)
    {
        conn_->shutdown();
        conn_.reset();
    }
}
