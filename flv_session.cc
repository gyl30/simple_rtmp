#include <boost/algorithm/string.hpp>
#include "flv_session.h"
#include "sink.h"
#include "log.h"

using simple_rtmp::flv_session;

flv_session::flv_session(std::string target, simple_rtmp::executors::executor& ex, boost::asio::ip::tcp::socket socket)
    : target_(std::move(target)), ex_(ex), conn_(std::make_shared<tcp_connection>(ex_, std::move(socket)))
{
}

void flv_session::start()
{
    std::vector<std::string> results;
    boost::split(results, target_, boost::is_any_of("/"));
    if (results.size() != 3)
    {
        shutdown();
        return;
    }
    std::string app = results[1];
    std::string stream = results[2];
    std::string id = "flv_" + app + "_" + stream;

    auto s = sink::get(id);
    if (!s)
    {
        LOG_ERROR("not found sink {}", id);
        shutdown();
        return;
    }
    sink_ = s;
    s->add_channel(channel_);

    channel_ = std::make_shared<simple_rtmp::channel>();
    channel_->set_output(std::bind(&flv_session::channel_out, shared_from_this(), std::placeholders::_1, std::placeholders::_2));

    conn_->set_read_cb(std::bind(&flv_session::on_read, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->set_write_cb(std::bind(&flv_session::on_write, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    conn_->start();
}

void flv_session::channel_out(const frame_buffer::ptr& frame, const boost::system::error_code& ec)
{
    if (ec)
    {
        LOG_ERROR("channel out {} failed {}", static_cast<void*>(this), ec.message());
        shutdown();
        return;
    }
    conn_->write_frame(frame);
}

void flv_session::shutdown()
{
    auto self = shared_from_this();

    ex_.post([this, self]() { safe_shutdown(); });
}

void flv_session::safe_shutdown()
{
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

void flv_session::on_read(const simple_rtmp::frame_buffer::ptr& frame, boost::system::error_code ec)
{
    (void)frame;
    if (ec)
    {
        shutdown();
        return;
    }
}

void flv_session::on_write(const boost::system::error_code& ec, std::size_t bytes)
{
    (void)bytes;
    if (ec)
    {
        shutdown();
        return;
    }
}
