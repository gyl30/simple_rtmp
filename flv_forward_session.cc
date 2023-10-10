#include "log.h"
#include "sink.h"
#include "flv_forward_session.h"

using simple_rtmp::flv_forward_session;
using simple_rtmp::tcp_connection;

flv_forward_session::flv_forward_session(std::string target, simple_rtmp::executors::executor& ex) : target_(std::move(target)), ex_(ex), conn_(std::make_shared<tcp_connection>(ex_))
{
}

void flv_forward_session::start()
{
    auto pos = target_.find_last_of('/');
    if (pos == std::string::npos)
    {
        shutdown();
        return;
    }

    std::string id = target_.substr(pos);
    auto s = sink::get(id);
    if (!s)
    {
        LOG_ERROR("not found sink {}", id);
        shutdown();
        return;
    }

    channel_ = std::make_shared<simple_rtmp::channel>();
    channel_->set_output(std::bind(&flv_forward_session::channel_out, shared_from_this(), std::placeholders::_1, std::placeholders::_2));

    sink_ = s;
    s->add_channel(channel_);
}

void flv_forward_session::shutdown()
{
    ex_.post(std::bind(&flv_forward_session::safe_shutdown, shared_from_this()));
}

boost::asio::ip::tcp::socket& flv_forward_session::socket()
{
    return conn_->socket();
}

void flv_forward_session::safe_shutdown()
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

void flv_forward_session::channel_out(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        LOG_ERROR("channel out {} failed {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
    conn_->write_frame(frame);
}
