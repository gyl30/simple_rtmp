#include <boost/algorithm/string.hpp>
#include <boost/utility/string_view.hpp>
#include "flv_session.h"
#include "sink.h"
#include "log.h"

using simple_rtmp::flv_session;

flv_session::flv_session(std::string target, simple_rtmp::executors::executor& ex, boost::asio::ip::tcp::socket socket)
    : target_(std::move(target)), ex_(ex), conn_(std::make_shared<tcp_connection>(ex_, std::move(socket)))
{
    LOG_DEBUG("create {}", target_);
}

flv_session::~flv_session()
{
    LOG_DEBUG("destory {}", target_);
}

std::string make_session_id_suffix(const std::string& target)
{
    //
    boost::string_view v(target);
    v.remove_suffix(4);
    std::vector<std::string> results;
    boost::split(results, v, boost::is_any_of("/"));
    if (results.size() < 2)
    {
        return "";
    }
    auto app_index = results.size() - 2;
    auto stream_index = results.size() - 1;
    std::string app = results[app_index];
    std::string stream = results[stream_index];
    return "flv_" + app + "_" + stream;
}

std::string make_session_id_prefix(const std::string& target)
{
    //
    std::vector<std::string> results;
    boost::split(results, target, boost::is_any_of("/"));
    if (results.size() < 3)
    {
        return "";
    }
    auto app_index = results.size() - 2;
    auto stream_index = results.size() - 1;
    std::string app = results[app_index];
    std::string stream = results[stream_index];
    return "flv_" + app + "_" + stream;
}

void flv_session::start()
{
    std::string id;
    if (boost::ends_with(target_, ".flv"))
    {
        id = make_session_id_suffix(target_);
    }
    else if (boost::starts_with(target_, "/flv"))
    {
        id = make_session_id_prefix(target_);
    }
    else
    {
        LOG_ERROR("invalid flv target {}", target_);
        shutdown();
        return;
    }

    auto s = sink::get(id);
    if (!s)
    {
        LOG_ERROR("not found sink {}", id);
        shutdown();
        return;
    }
    LOG_DEBUG("flv session {}", id);
    sink_ = s;

    channel_ = std::make_shared<simple_rtmp::channel>();
    channel_->set_output(std::bind(&flv_session::channel_out, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
    s->add_channel(channel_);

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
